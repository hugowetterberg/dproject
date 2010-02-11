<?php

function _dproject_release_information($release) {
  drupal_add_css(drupal_get_path('module', 'dproject') . '/css/admin.css');

  $packager = DProjectPackager::load($release->getPackager());  

  return theme('dproject_release_info', $release, $packager);
}