<?php
// $Id$

/**
 * Implementation of hook_cron
 */
function _dproject_cron() {

  // Fetch five temporary releases that are older than two hours
  $res = db_query_range("SELECT * FROM {dproject_release}
    WHERE temporary = 1 AND created < %d",
    strtotime('-2 hour'), 0, 5);

  // Use the static function for loading releases from a result pointer
  while ($release = DProjectRelease::loadFromResult($res)) {
    // Log the deletion
    $project = $release->getProjectNode();
    watchdog('dproject', 'The temporary !project release @version was deleted during cron run cleanup',
      l($project->title, 'node/' . $project->nid), $release->versionString());
    // Delete the release
    $release->delete();
  }
}