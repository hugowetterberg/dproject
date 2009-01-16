<?php
/**
 * Field handler to present a readable version number for the release.
 */
class dproject_views_handler_field_release_version extends views_handler_field {
  function construct() {
    parent::construct();
    $this->additional_fields['api_version'] = 'api_version';
    $this->additional_fields['major_version'] = 'major_version';
    $this->additional_fields['minor_version'] = 'minor_version';
    $this->additional_fields['version_extra'] = 'version_extra';
  }

  function query() {
    $this->ensure_my_table();
    $this->add_additional_fields();
  }

  function render($values) {
    $api = $values->{$this->aliases['api_version']};
    $major = $values->{$this->aliases['major_version']};
    $minor = $values->{$this->aliases['minor_version']};
    $extra = $values->{$this->aliases['version_extra']};

    if ($minor === NULL) {
      $minor = 'x';
    }

    $extra = empty($extra) ? '' : '-' . $extra;

    return $api . '.x-' . $major . '.' . $minor . $extra;
  }
}