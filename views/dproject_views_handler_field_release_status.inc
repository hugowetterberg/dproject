<?php
// $Id$

/**
 * @file
 *  Views integration for dproject releases
 */

/**
 * Field handler to present a readable status.
 */
class dproject_views_handler_field_release_status extends views_handler_field {
  function render($values) {
    $value = '';
    switch ($values->{$this->field_alias}) {
      case DProjectRelease::RECOMMENDED:
        $value = t('Recommended');
        break;
      case DProjectRelease::SUPPORTED:
        $value = t('Supported');
        break;
      case DProjectRelease::UNSUPPORTED:
        $value = t('Unsupported');
        break;
    }
    return $value;
  }
}