ARG ENVIRONMENT

FROM ubuntu:18.04 AS base

ARG wazuhbranch

RUN apt-get update && apt-get install -y supervisor
ADD base/wazuh-manager/supervisord.conf /etc/supervisor/conf.d/

RUN apt-get update && apt-get install python git gnupg2 gcc make vim libc6-dev curl policycoreutils automake autoconf libtool apt-transport-https lsb-release python-cryptography -y && curl -s https://packages.wazuh.com/key/GPG-KEY-WAZUH | apt-key add - && echo "deb https://s3-us-west-1.amazonaws.com/packages-dev.wazuh.com/staging/apt/ unstable main" | tee -a /etc/apt/sources.list.d/wazuh.list

RUN git clone https://github.com/wazuh/wazuh && cd /wazuh && git checkout $wazuhbranch
COPY base/wazuh-manager/preloaded-vars.conf /wazuh/etc/preloaded-vars.conf
RUN /wazuh/install.sh
RUN sed -i 's,"mode": \("white"\|"black"\),"mode": "black",g' /var/ossec/framework/python/lib/python3.7/site-packages/api-3.11.0-py3.7.egg/api/configuration.py
#####

COPY configurations/base/wazuh-master/config/ossec.conf /var/ossec/etc/ossec.conf
COPY configurations/base/wazuh-master/config/test.keys /var/ossec/etc/client.keys
COPY configurations/base/wazuh-master/config/agent-groups /var/ossec/queue/agent-groups
COPY configurations/base/wazuh-master/config/shared /var/ossec/etc/shared
COPY configurations/base/wazuh-master/config/agent-info /var/ossec/queue/agent-info
COPY configurations/base/wazuh-master/healthcheck/healthcheck.py /tmp/healthcheck.py
COPY configurations/base/wazuh-master/healthcheck/agent_control_check.txt /tmp/agent_control_check.txt
ADD base/wazuh-manager/entrypoint.sh /scripts/entrypoint.sh

FROM base AS wazuh-env-base
FROM base AS wazuh-env-ciscat
FROM base AS wazuh-env-syscollector

FROM base AS wazuh-env-security
COPY configurations/security/wazuh-master/rbac.db /var/ossec/api/configuration/security/rbac.db

FROM base AS wazuh-env-manager
COPY configurations/manager/wazuh-master/ossec-totals-27.log /var/ossec/stats/totals/2019/Aug/ossec-totals-27.log

FROM base AS wazuh-env-cluster
COPY configurations/cluster/wazuh-master/ossec-totals-27.log /var/ossec/stats/totals/2019/Aug/ossec-totals-27.log

FROM base as wazuh-env-security_white_rbac
COPY configurations/rbac/security/rbac.db /var/ossec/api/configuration/security/rbac.db
ADD configurations/rbac/security/white_configuration_rbac.sh /scripts/configuration_rbac.sh
RUN /scripts/configuration_rbac.sh
COPY configurations/base/wazuh-master/healthcheck/healthcheck_daemons.py /tmp/healthcheck.py
COPY configurations/base/wazuh-master/healthcheck/daemons_check.txt /tmp/daemons_check.txt

FROM base as wazuh-env-security_black_rbac
COPY configurations/rbac/security/rbac.db /var/ossec/api/configuration/security/rbac.db
ADD configurations/rbac/security/black_configuration_rbac.sh /scripts/configuration_rbac.sh
RUN /scripts/configuration_rbac.sh
COPY configurations/base/wazuh-master/healthcheck/healthcheck_daemons.py /tmp/healthcheck.py
COPY configurations/base/wazuh-master/healthcheck/daemons_check.txt /tmp/daemons_check.txt

FROM base as wazuh-env-agents_white_rbac
ADD configurations/rbac/agents/white_configuration_rbac.sh /scripts/configuration_rbac.sh
RUN /scripts/configuration_rbac.sh

FROM base as wazuh-env-agents_black_rbac
ADD configurations/rbac/agents/black_configuration_rbac.sh /scripts/configuration_rbac.sh
RUN /scripts/configuration_rbac.sh

FROM base as wazuh-env-ciscat_white_rbac
ADD configurations/rbac/ciscat/white_configuration_rbac.sh /scripts/configuration_rbac.sh
RUN /scripts/configuration_rbac.sh

FROM base as wazuh-env-ciscat_black_rbac
ADD configurations/rbac/ciscat/black_configuration_rbac.sh /scripts/configuration_rbac.sh
RUN /scripts/configuration_rbac.sh

FROM base as wazuh-env-rules_white_rbac
ADD configurations/rbac/rules/white_configuration_rbac.sh /scripts/configuration_rbac.sh
RUN /scripts/configuration_rbac.sh
COPY configurations/base/wazuh-master/healthcheck/healthcheck_daemons.py /tmp/healthcheck.py
COPY configurations/base/wazuh-master/healthcheck/daemons_check.txt /tmp/daemons_check.txt

FROM base as wazuh-env-rules_black_rbac
ADD configurations/rbac/rules/black_configuration_rbac.sh /scripts/configuration_rbac.sh
RUN /scripts/configuration_rbac.sh
COPY configurations/base/wazuh-master/healthcheck/healthcheck_daemons.py /tmp/healthcheck.py
COPY configurations/base/wazuh-master/healthcheck/daemons_check.txt /tmp/daemons_check.txt

FROM wazuh-env-${ENVIRONMENT}

HEALTHCHECK --retries=30 --interval=10s --timeout=30s --start-period=30s CMD /usr/bin/python3 /tmp/healthcheck.py || exit 1