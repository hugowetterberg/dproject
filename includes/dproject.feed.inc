<?php
// $Id$

/**
 * @file
 *  Functions for working with project feeds
 */

if (!defined('XML_NS')) {
  define('XML_NS', 'http://www.w3.org/2000/xmlns/');
}

if (!defined('XML_DC_NS')) {
  define('XML_DC_NS', 'http://purl.org/dc/elements/1.1/');
}

/**
 * Generates a release feed for the given project and api
 *
 * @param int|stdClass $pid
 *  The id of the project or project node to generate feed for
 * @param int $api_version
 *  The drupal version to generate feed for
 * @return string
 *  The contents of the feed
 */
function _dproject_feed_generate($pid, $api_version) {
  if (is_object($pid)) {
    $node = $pid;
    $pid = $project->nid;
  }
  else {
    $node = node_load($pid);
  }

  if (is_string($api_version)) {
    $api_version = intval($api_version);
  }

  $dp = DProject::singleton();

  // Get the latest release to calculate mod-date and E-tag
  $modified = $node->changed;
  $latest_releases = $dp->getReleases($node, $api_version, 1);
  if (!empty($latest_releases)) {
    $latest_release = $latest_releases[0];
    $modified = max($latest_release->getCreated(), $modified);
  }

  $no_change = FALSE;
  $no_change = $no_change || (isset($_SERVER['HTTP_IF_NONE_MATCH']) && $_SERVER['HTTP_IF_NONE_MATCH']==$modified);
  if (!$no_change && isset($_SERVER['HTTP_IF_MODIFIED_SINCE'])) {
    $since = strtotime($_SERVER['HTTP_IF_MODIFIED_SINCE']);
    $no_change = $since < $modified;
  }

  if ($no_change) {
    header('HTTP/1.1 304 Not Modified');
    exit;
  }

  $dom = new DomDocument('1.0', 'utf-8');
  $dom->formatOutput = TRUE;

  $project = $dom->createElement('project');
  $project->setAttributeNS(XML_NS, 'xmlns:dc', XML_DC_NS);
  $project->setAttributeNS(XML_NS, 'xmlns:dproject', DProject::NAMESPACE);
  $dom->appendChild($project);
  module_invoke_all('dproject_feed_project_added', $node, $project);

  $project->appendChild($dom->createElement('title', $node->title));
  $project->appendChild($dom->createElement('short_name', $node->short_name));
  $project->appendChild($dom->createElementNS(DProject::NAMESPACE, 'dproject:nid', $node->nid));
  $author = user_load($node->uid);
  if ($author) {
    $project->appendChild($dom->createElementNS(XML_DC_NS, 'dc:creator', $author->name));
  }
  $project->appendChild($dom->createElement('api_version', $api_version . '.x'));

  $recommended = $dp->getRecommendedRelease($node, $api_version);
  if ($recommended) {
    $rv = $recommended->getVersion();
    $project->appendChild($dom->createElement('recommended_major', $rv[1]));
  }

  $supported = $dp->getSupportedMajorVersions($node, $api_version);
  if ($supported) {
    $project->appendChild($dom->createElement('supported_major', join(',', $supported)));
  }

  $project->appendChild($dom->createElement('project_status', $node->status?'published':'unpublished'));
  $project->appendChild($dom->createElement('link', url('node/' . $node->nid, array('absolute' => TRUE))));

  $releases = $project->appendChild($dom->createElement('releases'));
  $rel_objects = $dp->getReleases($node, $api_version, 9, 1);
  if ($latest_release) {
    array_unshift($rel_objects, $latest_release);
  }

  foreach ($rel_objects as $rel) {
    $release = $releases->appendChild($dom->createElement('release'));
    $version = $rel->getVersion();
    $release->appendChild($dom->createElement('name', $node->short_name . ' ' . $rel->versionString()));
    $release->appendChild($dom->createElement('version', $rel->versionString()));
    $release->appendChild($dom->createElement('version_major', $version[1]));
    if (is_numeric($version[2])) {
      $release->appendChild($dom->createElement('version_patch', $version[2]));
    }
    if (!empty($version[3])) {
      $release->appendChild($dom->createElement('version_extra', $version[3]));
    }
    $release->appendChild($dom->createElement('status', $rel->getPublished()?'published':'unpublished'));
    // TODO: This release link doesn't point to anything. Implement it in the menu system.
    $release->appendChild($dom->createElement('release_link', url('project/' . $node->short_name . '/releases/' . $rel->versionString(), array('absolute' => TRUE))));
    $release->appendChild($dom->createElement('download_link', url($rel->getFilePath(), array('absolute' => TRUE))));
    $release->appendChild($dom->createElement('date', $rel->getCreated()));
    $release->appendChild($dom->createElementNS(DProject::NAMESPACE, 'iso_date', date('c', $rel->getCreated())));
    $release->appendChild($dom->createElement('mdhash', $rel->getMd5()));

    $terms = $release->appendChild($dom->createElement('terms'));
    $metadata = $rel->getMetadataObject();
    $release_terms = array();
    $release_types = $dp->releaseTypes(FALSE);
    if ($metadata->terms) {
      foreach ($metadata->terms as $term) {
        if (isset($release_types[$term])) {
          _dproject_feed_add_term($dom, $terms, $release_types[$term]);
        }
        else {
          _dproject_feed_add_term($dom, $terms, $term, FALSE);
        }
      }
    }

    $mdata_list = $rel->getMetadataList();
    foreach ($mdata_list as $md) {
      // Register the metadata namespace if needed. Use the existing
      // prefix if it's defined.
      $prefix = $dom->lookupPrefix($md->namespace);
      if ($prefix) {
        $md->name = $prefix;
      }
      else {
        $project->setAttributeNS(XML_NS, 'xmlns:' . $md->name, $md->namespace);
      }
      _dproject_feed_add_metadata($dom, $release, $md);
    }
  }

  header('Last-Modified: ' . date('r', $modified));
  header('Expires: ' . date('r', $modified+60));
  header('ETag: ' . $modified);
  header('Cache-Control: max-age=60, public');
  header('Content-Type: text/xml; charset=utf-8');
  print $dom->saveXML();
  die;
}

function _dproject_feed_add_metadata($dom, $release, $md) {
  $metadata = $release->appendChild($dom->createElementNS($md->namespace, $md->name . ':metadata'));
  $metadata->appendChild($dom->createCDATASection(json_encode($md->data)));
}

function _dproject_feed_add_term($dom, $terms, $value, $release_type=TRUE) {
  if (!$release_type) {
    return $terms->appendChild($dom->createElementNS(DProject::NAMESPACE, 'dproject:tag', $value));
  }

  $term = $terms->appendChild($dom->createElement('term'));
  $term->appendChild($dom->createElement('name', 'Release type'));
  $term->appendChild($dom->createElement('value', $value));
}