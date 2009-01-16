<?php
// $Id$

/**
 * @file
 *
 * Provide views data and handlers for dproject.module
 */

/**
 * Implementation of hook_views_data().
 */
function dproject_views_data() {
  $data = array();

//TODO: Status, Packager, Metadata, readable version

  $data['dproject_release']['table']['group']  = t('dProject Release');

  $data['dproject_release']['table']['base'] = array(
    'field' => 'rid',
    'title' => t('Release'),
    'help' => t('Release items are created from project nodes.'),
  );

  $data['dproject_release']['table']['join'] = array(
    'node' => array(
      'left_field' => 'nid',
      'field' => 'project_nid',
    ),
  );

  $data['dproject_release']['rid'] = array(
    'title' => t('Release ID'),
    'help' => t('The release ID of the release.'),
    'filter' => array(
      'handler' => 'views_handler_filter_numeric',
    ),
    'sort' => array(
      'handler' => 'views_handler_sort',
    ),
  );

  $data['dproject_release']['api_version'] = array(
    'title' => t('API Version'),
    'help' => t('The version of Drupal that the module supports.'),
    'field' => array(
      'handler' => 'views_handler_field_numeric',
      'click sortable' => TRUE,
    ),
    'argument' => array(
      'handler' => 'views_handler_argument_numeric',
    ),
    'filter' => array(
      'handler' => 'views_handler_filter_numeric',
    ),
    'sort' => array(
      'handler' => 'views_handler_sort',
    ),
  );

  $data['dproject_release']['major_version'] = array(
    'title' => t('Major Version'),
    'help' => t('The major version of the module.'),
    'field' => array(
      'handler' => 'views_handler_field_numeric',
      'click sortable' => TRUE,
    ),
    'argument' => array(
      'handler' => 'views_handler_argument_numeric',
    ),
    'filter' => array(
      'handler' => 'views_handler_filter_numeric',
    ),
    'sort' => array(
      'handler' => 'views_handler_sort',
    ),
  );

  $data['dproject_release']['minor_version'] = array(
    'title' => t('Minor Version'),
    'help' => t('The minor version of the module.'),
    'field' => array(
      'handler' => 'views_handler_field_numeric',
      'click sortable' => TRUE,
    ),
    'argument' => array(
      'handler' => 'views_handler_argument_numeric',
    ),
    'filter' => array(
      'handler' => 'views_handler_filter_numeric',
    ),
    'sort' => array(
      'handler' => 'views_handler_sort',
    ),
  );

  $data['dproject_release']['version_extra'] = array(
    'title' => t('Version Extra'),
    'help' => t('Extra version information'),
    'field' => array(
      'handler' => 'views_handler_field',
      'click sortable' => TRUE,
    ),
    'argument' => array(
      'handler' => 'views_handler_argument_string',
    ),
    'filter' => array(
      'handler' => 'views_handler_filter_string',
    ),
    'sort' => array(
      'handler' => 'views_handler_sort',
    ),
  );

  $data['dproject_release']['created'] = array(
    'title' => t('Created'),
    'help' => t('When the release was created'),
    'field' => array(
      'handler' => 'views_handler_field_date',
      'click sortable' => TRUE,
    ),
    'argument' => array(
      'handler' => 'views_handler_argument_date',
    ),
    'filter' => array(
      'handler' => 'views_handler_filter_date',
    ),
    'sort' => array(
      'handler' => 'views_handler_sort_date',
    ),
  );

  $data['dproject_release']['published'] = array(
    'title' => t('Published'),
    'help' => t('The published state of the release'),
    'field' => array(
      'handler' => 'views_handler_field_boolean',
      'click sortable' => TRUE,
    ),
    'filter' => array(
      'handler' => 'views_handler_filter_boolean_operator',
      'label' => t('Published'),
      'type' => 'yes-no',
    ),
    'sort' => array(
      'handler' => 'views_handler_sort',
    ),
  );

  $data['dproject_release']['temporary'] = array(
    'title' => t('Temporary'),
    'help' => t('Whether this is a temporary release'),
    'field' => array(
      'handler' => 'views_handler_field_boolean',
      'click sortable' => TRUE,
    ),
    'filter' => array(
      'handler' => 'views_handler_filter_boolean_operator',
      'label' => t('Temporary'),
      'type' => 'yes-no',
    ),
    'sort' => array(
      'handler' => 'views_handler_sort',
    ),
  );

  $data['dproject_release']['file_path'] = array(
    'title' => t('File Path'),
    'help' => t('Path to where the file can be found'),
    'field' => array(
      'handler' => 'views_handler_field',
      'click sortable' => TRUE,
    ),
    'argument' => array(
      'handler' => 'views_handler_argument_string',
    ),
    'filter' => array(
      'handler' => 'views_handler_filter_string',
    ),
    'sort' => array(
      'handler' => 'views_handler_sort',
    ),
  );

  $data['dproject_release']['file_path'] = array(
    'title' => t('Release Path'),
    'help' => t('Path to where the release can be found'),
    'field' => array(
      'handler' => 'views_handler_field',
      'click sortable' => TRUE,
    ),
    'argument' => array(
      'handler' => 'views_handler_argument_string',
    ),
    'filter' => array(
      'handler' => 'views_handler_filter_string',
    ),
    'sort' => array(
      'handler' => 'views_handler_sort',
    ),
  );

  $data['dproject_release']['md5'] = array(
    'title' => t('MD5'),
    'help' => t('The md5 hash of the release file'),
    'field' => array(
      'handler' => 'views_handler_field',
      'click sortable' => TRUE,
    ),
    'argument' => array(
      'handler' => 'views_handler_argument_string',
    ),
    'filter' => array(
      'handler' => 'views_handler_filter_string',
    ),
    'sort' => array(
      'handler' => 'views_handler_sort',
    ),
  );

  /* Non-table fields */

  $data['dproject_release']['view_release'] = array(
    'title' => t('Release Link'),
    'help' => t('Link to where the release can be found'),
    'field' => array(
      'handler' => 'dproject_views_handler_field_release_link',
    ),
  );

  return $data;
}

/**
 * Implementation of hook_views_handlers() to register all of the basic handlers
 * views uses.
 */
function dproject_views_handlers() {
  return array(
    'info' => array(
      'path' => drupal_get_path('module', 'dproject') . '/views',
    ),
    'handlers' => array(
      'dproject_views_handler_field_release_link' => array(
        'parent' => 'views_handler_field',
      ),
    ),
  );
}
