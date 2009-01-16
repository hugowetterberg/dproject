<?php
/**
 * Field handler to present a readable status.
 */
class dproject_views_handler_field_release_status extends views_handler_field {
  function render($values) {
    $value = '';
    switch ($values->{$this->field_alias}) {
      case DPROJECT_RELEASE_STATUS_RECOMMENDED:
        $value = t('Recommended');
        break;
      case DPROJECT_RELEASE_STATUS_SUPPORTED:
        $value = t('Supported');
        break;
      case DPROJECT_RELEASE_STATUS_UNSUPPORTED:
        $value = t('Unsupported');
        break;
    }
    return $value;
  }
}