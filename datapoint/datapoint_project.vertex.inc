<?php
// $Id$

/**
 * @file
 *  Implementation of the vertex_schema hook for the datapoint_project module.
 */

function datapoint_project_vertex_schema() {
  return array(
    'description' => t('Releases from a project server feed'),
    'fields' => array(
      'major_version' => array(
        'description' => t('The major release version'),
        'type' => 'int',
        'unsigned' => TRUE,
        'not null' => TRUE,
        'default' => 1,
      ),
      'minor_version' => array(
        'description' => t('The minor release version'),
        'type' => 'int',
        'unsigned' => TRUE,
        'not null' => FALSE,
        'default' => NULL,
      ),
      'version_extra' => array(
        'description' => t('Extra version information'),
        'type' => 'varchar',
        'length' => 32,
        'not null' => TRUE,
        'default' => '',
      ),
      'status' => array(
        'description' => t('The status of the release'),
        'type' => 'varchar',
        'length' => 32,
        'not null' => TRUE,
        'default' => '',
      ),
      'release_url' => array(
        'description' => t('The url to the release description'),
        'type' => 'varchar',
        'length' => 255,
        'not null' => TRUE,
        'default' => '',
      ),
      'file_url' => array(
        'description' => t('The url to the release file'),
        'type' => 'varchar',
        'length' => 255,
        'not null' => TRUE,
        'default' => '',
      ),
      'md5' => array(
        'description' => t('The md5 hash of the release file'),
        'type' => 'varchar',
        'length' => 32,
        'not null' => TRUE,
        'default' => '',
      ),
      'release_terms' => array(
        'description' => t('Terms describing the release encoded as json'),
        'type' => 'text',
        'size' => 'big',
      ),
      'release_metadata' => array(
        'description' => t('Metadata describing the release encoded as json'),
        'type' => 'text',
        'size' => 'big',
      ),
    ),
  );
}