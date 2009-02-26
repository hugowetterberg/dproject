<?php
// $Id$

function _dproject_settings() {
  $release_type_filter = variable_get('dproject_valid_release_source_types', DProjectPackager::SOURCES_DEFAULT);

  $vrest_opts = array(
    DProjectPackager::SOURCES_TRACKABLE => t('Non-static sources'),
    DProjectPackager::SOURCES_STATIC => t('Static sources'),
    DProjectPackager::SOURCES_EXPLICIT => t('Only explicit sources'),
  );

  // Create an array of default values through comparison with binary and
  $vrest_def = array();
  foreach ($vrest_opts as $type => $title) {
    if (($release_type_filter & $type) == $type) {
      $vrest_def[$type] = $type;
    }
  }

  $form = array(
    'valid_release_source_types' => array(
      '#type' => 'checkboxes',
      '#title' => t('Valid release source types'),
      '#options' => $vrest_opts,
      '#default_value' => $vrest_def,
      '#description' => t('A example of a non static source would be a source that contains branches or head/trunk in a VCS system. A static source could be a listing of recent commits, which would not be a explicit source, as commits aren\'t explicitly created like branches and tags. Tags are a example of a source that\'s both static and explicit, making it a natural choice as a release source.'),
    ),
  );

  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => 'Save configuration',
  );

  return $form;
}

function _dproject_settings_submit($form, $form_state) {
  // Create composite value through binary or
  $vrest = 0;
  foreach ($form_state['values']['valid_release_source_types'] as $key => $checked) {
    if ($checked) {
      $vrest |= $key;
    }
  }
  variable_set('dproject_valid_release_source_types', $vrest);
  drupal_set_message('The DProject settings have been updated');
}