<?php
// $Id$

/**
 * @file
 *  Views integration for dproject releases
 */

/**
 * Argument handler to accept status.
 */
class dproject_views_handler_argument_release_status extends views_handler_argument {
  /**
   * Override the behavior of summary_name(). Get the user friendly version
   * of the status.
   */
  function summary_name($data) {
    return $this->release_status($data->{$this->name_alias});
  }

  /**
   * Override the behavior of title(). Get the user friendly version of the
   * status.
   */
  function title() {
    return $this->release_status($this->argument);
  }

  function release_status($status) {
    $value = '';
    switch ($status) {
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
