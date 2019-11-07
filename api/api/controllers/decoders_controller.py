# Copyright (C) 2015-2019, Wazuh Inc.
# Created by Wazuh, Inc. <info@wazuh.com>.
# This program is a free software; you can redistribute it and/or modify it under the terms of GPLv2

import asyncio
import logging

import connexion

from api.authentication import get_permissions
from api.util import remove_nones_to_dict, exception_handler, parse_api_param, raise_if_exc
from wazuh.cluster.dapi.dapi import DistributedAPI
from wazuh import decoder as decoder_framework

loop = asyncio.get_event_loop()
logger = logging.getLogger('wazuh')


@exception_handler
def get_decoders(decoder_names: list = None, pretty: bool = False, wait_for_complete: bool = False, offset: int = 0,
                 limit: int = None, sort: str = None, search: str = None, file: str = None, path: str = None, status: str = None):
    """Get all decoders

    Returns information about all decoders included in ossec.conf. This information include decoder's route,
    decoder's name, decoder's file among others

    :param decoder_names: Filters by decoder name.
    :param pretty: Show results in human-readable format
    :param wait_for_complete: Disable timeout response
    :param offset: First element to return in the collection
    :param limit: Maximum number of elements to return
    :param sort: Sorts the collection by a field or fields (separated by comma). Use +/- at the beginning to list in
    ascending or descending order.
    :param search: Looks for elements with the specified string
    :param file: Filters by filename.
    :param path: Filters by path
    :param status: Filters by list status.
    :return: Data object
    """
    f_kwargs = {'names': decoder_names, 'offset': offset, 'limit': limit, 'file': file, 'status': status, 'path': path,
                'sort_by': parse_api_param(sort, 'sort')['fields'] if sort is not None else ['file', 'position'],
                'sort_ascending': True if sort is None or parse_api_param(sort, 'sort')['order'] == 'asc' else False,
                'search_text': parse_api_param(search, 'search')['value'] if search is not None else None,
                'complementary_search': parse_api_param(search, 'search')['negation'] if search is not None else None}

    dapi = DistributedAPI(f=decoder_framework.get_decoders,
                          f_kwargs=remove_nones_to_dict(f_kwargs),
                          request_type='local_any',
                          is_async=False,
                          wait_for_complete=wait_for_complete,
                          pretty=pretty,
                          logger=logger,
                          rbac_permissions=get_permissions(connexion.request.headers['Authorization'])
                          )
    data = raise_if_exc(loop.run_until_complete(dapi.distribute_function()))

    return data, 200


@exception_handler
def get_decoders_files(pretty: bool = False, wait_for_complete: bool = False, offset: int = 0, limit: int = None,
                       sort: str = None, search: str = None, file: str = None, path: str = None, status: str = None):
    """Get all decoders files

    Returns information about all decoders files used in Wazuh. This information include decoder's file, decoder's route
    and decoder's status among others

    :param pretty: Show results in human-readable format
    :param wait_for_complete: Disable timeout response
    :param offset: First element to return in the collection
    :param limit: Maximum number of elements to return
    :param sort: Sorts the collection by a field or fields (separated by comma). Use +/- at the beginning to list in
    ascending or descending order.
    :param search: Looks for elements with the specified string
    :param file: Filters by filename.
    :param path: Filters by path
    :param status: Filters by list status.
    :return: Data object
    """
    f_kwargs = {'offset': offset,
                'limit': limit,
                'sort_by': parse_api_param(sort, 'sort')['fields'] if sort is not None else ['file'],
                'sort_ascending': True if sort is None or parse_api_param(sort, 'sort')['order'] == 'asc' else False,
                'search_text': parse_api_param(search, 'search')['value'] if search is not None else None,
                'complementary_search': parse_api_param(search, 'search')['negation'] if search is not None else None,
                'file': file,
                'path': path,
                'status': status}

    dapi = DistributedAPI(f=decoder_framework.get_decoders_files,
                          f_kwargs=remove_nones_to_dict(f_kwargs),
                          request_type='local_any',
                          is_async=False,
                          wait_for_complete=wait_for_complete,
                          pretty=pretty,
                          logger=logger,
                          rbac_permissions=get_permissions(connexion.request.headers['Authorization'])
                          )
    data = raise_if_exc(loop.run_until_complete(dapi.distribute_function()))

    return data, 200


@exception_handler
def get_download_file(pretty: bool = False, wait_for_complete: bool = False, file: str = None):
    """Download an specified decoder file.

    :param pretty: Show results in human-readable format
    :param wait_for_complete: Disable timeout response
    :param file: File name to download.
    :return: Raw xml file
    """
    f_kwargs = {'file': file}

    dapi = DistributedAPI(f=decoder_framework.get_file,
                          f_kwargs=remove_nones_to_dict(f_kwargs),
                          request_type='local_any',
                          is_async=False,
                          wait_for_complete=wait_for_complete,
                          pretty=pretty,
                          logger=logger,
                          rbac_permissions=get_permissions(connexion.request.headers['Authorization'])
                          )
    data = raise_if_exc(loop.run_until_complete(dapi.distribute_function()))
    response = connexion.lifecycle.ConnexionResponse(body=data["message"], mimetype='application/xml')

    return data


@exception_handler
def get_decoders_parents(pretty: bool = False, wait_for_complete: bool = False, offset: int = 0, limit: int = None,
                         sort: str = None, search: str = None):
    """Get decoders by parents

    Returns information about all parent decoders. A parent decoder is a decoder used as base of other decoders

    :param pretty: Show results in human-readable format
    :param wait_for_complete: Disable timeout response
    :param offset: First element to return in the collection
    :param limit: Maximum number of elements to return
    :param sort: Sorts the collection by a field or fields (separated by comma). Use +/- at the beginning to list in
    ascending or descending order.
    :param search: Looks for elements with the specified string
    :return: Data object
    """
    f_kwargs = {'offset': offset,
                'limit': limit,
                'sort_by': parse_api_param(sort, 'sort')['fields'] if sort is not None else ['file', 'position'],
                'sort_ascending': True if sort is None or parse_api_param(sort, 'sort')['order'] == 'asc' else False,
                'search_text': parse_api_param(search, 'search')['value'] if search is not None else None,
                'complementary_search': parse_api_param(search, 'search')['negation'] if search is not None else None,
                'parents': True}

    dapi = DistributedAPI(f=decoder_framework.get_decoders,
                          f_kwargs=remove_nones_to_dict(f_kwargs),
                          request_type='local_any',
                          is_async=False,
                          wait_for_complete=wait_for_complete,
                          pretty=pretty,
                          logger=logger,
                          rbac_permissions=get_permissions(connexion.request.headers['Authorization'])
                          )
    data = raise_if_exc(loop.run_until_complete(dapi.distribute_function()))

    return data, 200
