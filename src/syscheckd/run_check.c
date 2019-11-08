/* Copyright (C) 2015-2019, Wazuh Inc.
 * Copyright (C) 2010 Trend Micro Inc.
 * All right reserved.
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (version 2) as published by the FSF - Free Software
 * Foundation
 */


// SCHED_BATCH is Linux specific and is only picked up with _GNU_SOURCE
#ifdef __linux__
#include <sched.h>
#endif

#include "shared.h"
#include "syscheck.h"
#include "os_crypto/md5_sha1_sha256/md5_sha1_sha256_op.h"
#include "rootcheck/rootcheck.h"

// Prototypes
//static void send_sk_db(int first_scan);
void * fim_run_realtime(__attribute__((unused)) void * args);
int fim_whodata_initialize();


#ifdef WIN32
static void set_priority_windows_thread();
#elif defined INOTIFY_ENABLED
//static void *symlink_checker_thread(__attribute__((unused)) void * data);
//static void update_link_monitoring(int pos, char *old_path, char *new_path);
//static void unlink_files(OSHashNode **row, OSHashNode **node, void *data);
//static void send_silent_del(char *path);
#endif

// Send a message
static void fim_send_msg(char mq, const char * location, const char * msg) {
    if (SendMSG(syscheck.queue, msg, location, mq) < 0) {
        merror(QUEUE_SEND);

        if ((syscheck.queue = StartMQ(DEFAULTQPATH, WRITE)) < 0) {
            merror_exit(QUEUE_FATAL, DEFAULTQPATH);
        }

        // Try to send it again
        SendMSG(syscheck.queue, msg, location, mq);
    }
}

void fim_send_sync_msg(const char * msg) {
    mdebug2(FIM_DBSYNC_SEND, msg);
    fim_send_msg(DBSYNC_MQ, SYSCHECK, msg);
    struct timespec timeout = { syscheck.send_delay / 1000000, syscheck.send_delay % 1000000 * 1000 };
    nanosleep(&timeout, NULL);
}

// Send a message related to syscheck change/addition
void send_syscheck_msg(const char *msg)
{
#ifndef WIN32
    mdebug2(FIM_SEND, msg);
#endif
    fim_send_msg(SYSCHECK_MQ, SYSCHECK, msg);
    struct timespec timeout = { syscheck.send_delay / 1000000, syscheck.send_delay % 1000000 * 1000 };
    nanosleep(&timeout, NULL);
}

// Send a scan info event
void fim_send_scan_info(fim_scan_event event) {
    cJSON * json = fim_scan_info_json(event, time(NULL));
    char * plain = cJSON_PrintUnformatted(json);

    send_syscheck_msg(plain);

    free(plain);
    cJSON_Delete(json);
}

// Send a message related to logs
int send_log_msg(const char * msg)
{
    fim_send_msg(LOCALFILE_MQ, SYSCHECK, msg);
    return (0);
}

void start_daemon()
{
    int day_scanned = 0;
    int curr_day = 0;
    time_t curr_time = 0;
    time_t prev_time_sk = 0;
    char curr_hour[12];
    struct tm *p;

    // Some time to settle
    memset(curr_hour, '\0', 12);
    sleep(syscheck.tsleep);
    minfo(FIM_DAEMON_STARTED);

    // A higher nice value means a low priority.
#ifndef WIN32
    mdebug1(FIM_PROCESS_PRIORITY, syscheck.process_priority);

    if (nice(syscheck.process_priority) == -1) {
        merror(NICE_ERROR, strerror(errno), errno);
    }
#endif

#ifndef WIN32
    // Launch rootcheck thread
    w_create_thread(w_rootcheck_thread, &syscheck);
#else
    if (CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)w_rootcheck_thread,
            &syscheck, 0, NULL) == NULL) {
        merror(THREAD_ERROR);
    }
#endif

    // If the scan time/day is set, reset the syscheck.time/rootcheck.time
    if (syscheck.scan_time || syscheck.scan_day) {
        // At least once a week
        syscheck.time = 604800;
    }

    // Deleting content local/diff directory
    char *diff_dir;

    os_calloc(PATH_MAX, sizeof(char), diff_dir);
    snprintf(diff_dir, PATH_MAX, "%s/local/", DIFF_DIR_PATH);

    cldir_ex(diff_dir);

    if (syscheck.disabled) {
        return;
    }

    // Create File integrity monitoring base-line
    minfo(FIM_FREQUENCY_TIME, syscheck.time);
    fim_scan();
#ifndef WIN32
    // Launch Real-time thread
    w_create_thread(fim_run_realtime, &syscheck);

    // Launch inventory synchronization thread, if enabled
    if (syscheck.enable_inventory) {
        w_create_thread(fim_run_integrity, &syscheck);
    }

#else
    if (CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)fim_run_integrity,
            &syscheck, 0, NULL) == NULL) {
        merror(THREAD_ERROR);
    }
    if (CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)fim_run_realtime,
            &syscheck, 0, NULL) == NULL) {
        merror(THREAD_ERROR);
    }
#endif

    // Launch Whodata real-time thread
    fim_whodata_initialize();

    // Before entering in daemon mode itself
    prev_time_sk = time(0);

    // If the scan_time or scan_day is set, we need to handle the current day/time on the loop.
    if (syscheck.scan_time || syscheck.scan_day) {
        curr_time = time(0);
        p = localtime(&curr_time);

        // Assign hour/min/sec values
        snprintf(curr_hour, 9, "%02d:%02d:%02d",
                 p->tm_hour,
                 p->tm_min,
                 p->tm_sec);

        curr_day = p->tm_mday;

        if (syscheck.scan_time && syscheck.scan_day) {
            if ((OS_IsAfterTime(curr_hour, syscheck.scan_time)) &&
                    (OS_IsonDay(p->tm_wday, syscheck.scan_day))) {
                day_scanned = 1;
            }
        } else if (syscheck.scan_time) {
            if (OS_IsAfterTime(curr_hour, syscheck.scan_time)) {
                day_scanned = 1;
            }
        } else if (syscheck.scan_day) {
            if (OS_IsonDay(p->tm_wday, syscheck.scan_day)) {
                day_scanned = 1;
            }
        }
    }

    // Check every SYSCHECK_WAIT
    while (1) {
        int run_now = 0;
        curr_time = time(0);

        // Check if syscheck should be restarted
        run_now = os_check_restart_syscheck();

        // Check if a day_time or scan_time is set
        if (syscheck.scan_time || syscheck.scan_day) {
            p = localtime(&curr_time);

            // Day changed
            if (curr_day != p->tm_mday) {
                day_scanned = 0;
                curr_day = p->tm_mday;
            }

            // Check for the time of the scan
            if (!day_scanned && syscheck.scan_time && syscheck.scan_day) {
                // Assign hour/min/sec values
                snprintf(curr_hour, 9, "%02d:%02d:%02d",
                         p->tm_hour, p->tm_min, p->tm_sec);

                if ((OS_IsAfterTime(curr_hour, syscheck.scan_time)) &&
                        (OS_IsonDay(p->tm_wday, syscheck.scan_day))) {
                    day_scanned = 1;
                    run_now = 1;
                }
            } else if (!day_scanned && syscheck.scan_time) {
                // Assign hour/min/sec values
                snprintf(curr_hour, 9, "%02d:%02d:%02d",
                         p->tm_hour, p->tm_min, p->tm_sec);

                if (OS_IsAfterTime(curr_hour, syscheck.scan_time)) {
                    run_now = 1;
                    day_scanned = 1;
                }
            } else if (!day_scanned && syscheck.scan_day) {
                // Check for the day of the scan
                if (OS_IsonDay(p->tm_wday, syscheck.scan_day)) {
                    run_now = 1;
                    day_scanned = 1;
                }
            }
        }

        // If time elapsed is higher than the syscheck time, run syscheck time
        if (((curr_time - prev_time_sk) > syscheck.time) || run_now) {
            fim_scan();
            prev_time_sk = time(0);
        }
        sleep(SYSCHECK_WAIT);
    }
}


// Starting Real-time thread
void * fim_run_realtime(__attribute__((unused)) void * args) {

#if defined INOTIFY_ENABLED || defined WIN32

#ifdef WIN32
    set_priority_windows_thread();

    // Directories in Windows configured with real-time add recursive watches
    int i = 0;
    while (syscheck.dir[i]) {
        if (syscheck.opts[i] & REALTIME_ACTIVE) {
            realtime_adddir(syscheck.dir[i], 0);
        }
        i++;
    }
#endif

    while (1) {
        if (syscheck.realtime && (syscheck.realtime->fd >= 0)) {
            log_realtime_status(1);

#ifdef INOTIFY_ENABLED
            struct timeval selecttime;
            fd_set rfds;
            int run_now = 0;

            selecttime.tv_sec = SYSCHECK_WAIT;
            selecttime.tv_usec = 0;

            // zero-out the fd_set
            FD_ZERO (&rfds);
            FD_SET(syscheck.realtime->fd, &rfds);

            run_now = select(syscheck.realtime->fd + 1,
                            &rfds,
                            NULL,
                            NULL,
                            &selecttime);

            if (run_now < 0) {
                merror(FIM_ERROR_SELECT);
            } else if (run_now == 0) {
                // Timeout
            } else if (FD_ISSET (syscheck.realtime->fd, &rfds)) {
                realtime_process();
            }

#elif defined WIN32
            if (WaitForSingleObjectEx(syscheck.realtime->evt, SYSCHECK_WAIT * 1000, TRUE) == WAIT_FAILED) {
                merror(FIM_ERROR_REALTIME_WAITSINGLE_OBJECT);
            }
#endif
        } else {
            sleep(SYSCHECK_WAIT);
        }
    }

#else
    mwarn(FIM_WARN_REALTIME_UNSUPPORTED);
    pthread_exit(NULL);
#endif

}


#ifdef WIN32
void set_priority_windows_thread() {
    DWORD dwCreationFlags = syscheck.process_priority <= -10 ? THREAD_PRIORITY_HIGHEST :
                      syscheck.process_priority <= -5 ? THREAD_PRIORITY_ABOVE_NORMAL :
                      syscheck.process_priority <= 0 ? THREAD_PRIORITY_NORMAL :
                      syscheck.process_priority <= 5 ? THREAD_PRIORITY_BELOW_NORMAL :
                      syscheck.process_priority <= 10 ? THREAD_PRIORITY_LOWEST :
                      THREAD_PRIORITY_IDLE;

    mdebug1(FIM_PROCESS_PRIORITY, syscheck.process_priority);

    if(!SetThreadPriority(GetCurrentThread(), dwCreationFlags)) {
        int dwError = GetLastError();
        merror("Can't set thread priority: %d", dwError);
    }
}
#endif


int fim_whodata_initialize() {
#if defined INOTIFY_ENABLED || defined WIN32

#ifdef WIN32
    set_priority_windows_thread();
#endif

    for (int i = 0; syscheck.dir[i]; i++) {
        if (syscheck.opts[i] & WHODATA_ACTIVE) {
            realtime_adddir(syscheck.dir[i], i + 1);
        }
    }

#ifdef WIN_WHODATA
    HANDLE t_hdle;
    long unsigned int t_id;
    if (syscheck.wdata.whodata_setup && !run_whodata_scan()) {
        if (t_hdle = CreateThread(NULL, 0, state_checker, NULL, 0, &t_id), !t_hdle) {
            merror(FIM_ERROR_CHECK_THREAD);
            return -1;
        }
    }
#elif ENABLE_AUDIT
    audit_set_db_consistency();
#endif

#else
    mwarn(FIM_WARN_WHODATA_UNSUPPORTED);
#endif

    return 0;
}

void log_realtime_status(int next) {
    /*
     * 0: stop (initial)
     * 1: run
     * 2: pause
     */

    static int status = 0;

    switch (status) {
    case 0:
        if (next == 1) {
            minfo(FIM_REALTIME_STARTED);
            status = next;
        }
        break;
    case 1:
        if (next == 2) {
            minfo(FIM_REALTIME_PAUSED);
            status = next;
        }
        break;
    case 2:
        if (next == 1) {
            minfo(FIM_REALTIME_RESUMED);
            status = next;
        }
    }
}
