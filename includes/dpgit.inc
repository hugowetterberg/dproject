<?php
// $Id$

/**
 * @file
 *  Theme functions and page handlers that are lazy-loaded by the theme and menu system
 */

/**
 * Configuration form page handlers for dpgit
 *
 * @param array $form_state
 * @return array
 *  The configuration form
 */
function _dpgit_settings($form_state) {
  $form = array();
  $git_path = variable_get('git_path', 'git');
  $form['git_path'] = array(
    '#title' => t('Git path'),
    '#type' => 'textfield',
    '#default_value' => $git_path,
  );

  return system_settings_form($form);
}

/**
 * Theme function for git logs
 *
 * @param array $log
 *  A log array from _dpgit_log
 * @return string
 *  The themed log
 */
function _dpgit_theme_log($log) {
  $out = '<ol class="git-log">';
  foreach ($log as $sha1 => $item) {
    $out .= '<li class="commit">';
    $out .= theme('git_log_item', $sha1, $item);
    $out .= '</li>';
  }
  return $out . '</ol>';
}

/**
 * Theme function for git log items (commits)
 *
 * @param sting $sha1
 *  The $sha1 of the commit
 * @param array $item
 *  A git log log item
 * @return string
 *  The themed log item
 */
function _dpgit_theme_log_item($sha1, $item) {
  $out .= '<dl class="header">';
  $out .=   '<dt>sha1</dt><dd class="sha1">' . $sha1 . '</dd>';
  foreach ($item['header'] as $name => $value) {
    $out .= '<dt>' . $name . '</dt><dd>' . htmlspecialchars($value) . '</dd>';
  }
  $out .= '</dl>';
  $out .= '<div class="message">' . nl2br(htmlspecialchars($item['message'])) . '</div>';
  $out .= '<ul class="files">';
  foreach ($item['files'] as $file => $status) {
    $out .= '<li class="file file-' . drupal_strtolower($status) . '">' .
      '<span class="file-status">' . $status . '</span> ' .
      '<span class="file-name">' . htmlspecialchars($file) . '</span></li>';
  }
  $out .= '</ul>';
  return $out;
}