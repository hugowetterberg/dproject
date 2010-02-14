<?php
// $Id$

/**
 * @file
 *  Packager class that provides git integration for dproject
 */

/**
 * Git implementation of DProjectPackager
 *
 * @package dpgit
 */
class DPGitPackager extends DProjectPackager implements dProjectUpdateablePackager {
  const GIT_NAMESPACE = 'http://goodold.se/project/dproject/dpgit';

  /**
   * Gets the the type name of the packager
   *
   * @return void
   */
  public function getTypeName() {
    return 'git';
  }

  /**
   * Callback for getting the name of the git packager
   *
   * @return string
   *  A description
   */
  public function getName() {
    return t('Git repository');
  }

  /**
   * Callback for getting a description for the git packager
   *
   * @return string
   *  A description
   */
  public function getDescription() {
    return '<strong>' . t('Origin') . ':</strong> ' . $this->configuration['url'];
  }

  /**
   * Gets the description of a release's source
   *
   * @param DProjectRelease $release
   *  The the release object
   * @return string
   *  The description of the source
   */
  public function getSourceDescription($release) {
    $git_info = $release->getMetadataObject('git', DPGitPackager::GIT_NAMESPACE);

    if ($git_info) {
      $wd = $this->workingDirectory();
      $log = DPGitPackager::log($wd . '/.git', 1, $git_info->sha1, '.');

      $reference = $git_info->sha1;
      if (isset($git_info->reference)) {
        $reference = $git_info->reference;
      }
      $out = '<h2 class="source-reference">' . $reference . '</h2>';
      $out .= theme('git_log', $log);
      return $out;
    }
  }

  /**
   * Gets sources with optional name and type matching.
   *
   * @param string $name
   *  Optional. The name source to get.
   *  Set to NULL if you want all the sources.
   * @param int $filter
   *  Optional. The filter you want to apply to the sources. One, or a
   *  combination of the DProjectPackager::SOURCES_* flags. Defaults to 0.
   * @return array
   *  A an array of zero or more sources. Or a single source if a name was
   *  specified. If both name and filter was set and the named source didn't
   *  match the filter, NULL will be returned.
   */
  public function getSources($name=NULL, $filter=0) {
    $sources = array(
      'branches' => array(
        'title' => t('Branches'),
        'options' => array($this, 'branchesOptions'),
        'resolve' => array($this, 'referenceSha1'),
        'type' => DProjectPackager::SOURCES_TRACKABLE | DProjectPackager::SOURCES_EXPLICIT,
      ),
      'tags' => array(
        'title' => t('Tags'),
        'options' => array($this, 'tagsOptions'),
        'resolve' => array($this, 'referenceSha1'),
        'type' => DProjectPackager::SOURCES_STATIC | DProjectPackager::SOURCES_EXPLICIT,
      ),
      'recent_commits' => array(
        'title' => t('Recent commits'),
        'options' => array($this, 'recentCommitsOptions'),
        'resolve' => array($this, 'referenceSha1'),
        'type' => DProjectPackager::SOURCES_STATIC,
      ),
    );

    if ($name) {
      if (isset($sources[$name])) {
        list($source) = $this->filterSources(array($sources[$name]), $filter);
        return $source;
      }
    }
    else {
      return $this->filterSources($sources, $filter);
    }
  }

  /**
   * Callback for generating a form fragment for configuring
   * a new git packager.
   *
   * @return array
   *  A form fragment
   */
  public function configurationForm() {
    $form = array();
    $form['git_source'] = array(
      '#type' => 'textfield',
      '#required' => TRUE,
      '#title' => t('The url or path to the git repository'),
    );
    return $form;
  }

  /**
   * Callback for turning the submitted values for the configuration form
   * into a configuration array
   *
   * @param string $values
   * @return array
   *  A configuration array
   */
  public function configurationFormSubmit($values) {
    return array(
      'url' => $values['git_source'],
    );
  }

  /**
   * Configures a release object using the supplied information
   *
   * @param DProjectRelease $release
   *  The release object to configure
   * @param string $reference
   *  The reference that was used (tag, branch, sha1 and so on)
   * @return void
   */
  public function configureRelease(&$release, $reference) {
    $metadata = $release->getMetadataObject('git', DPGitPackager::GIT_NAMESPACE);
    $metadata->origin = $this->conf('url');

    $wd = $this->workingDirectory();
    $info = DPGitPackager::resolveReference($wd . '/.git', $reference);
    $release->addFileAlter('DPGitPackager::expandKeywords');
    $release->addFileExclude('/\.git.*/');

    if ($info) {
      $sha1 = $info['sha1'];
      $metadata->sha1 = $sha1;
      if ($reference != $sha1) {
        $metadata->reference = $reference;
      }
      $metadata->info = $info;
    }
  }

  /**
   * Creates a local clone of the git repository
   * @return void
   */
  public function created(&$status_out=Null) {
    $wdir = $this->workingDirectory();
    if (file_exists(dirname($wdir)) || mkdir(dirname($wdir), 0777, TRUE)) {
      // Make sure that the working dir doesn't exist (by killing it if it does)
      if (file_exists($wdir)) {
        DProjectPackager::deleteDirectory($wdir);
      }

      $out = array();
      $return = 0;
      $cmd = DPGitPackager::gitShellCommand('clone "' . $this->conf('url') . '" "' . $wdir . '"');
      exec($cmd, $out, $return);

      if ($return === 0) {
        drupal_set_message(t('Git: Initialized empty Git repository in !dir', array('!dir' => $wdir)));
        if ($status_out) {
          $status_out = join($out, '<br/>');
        }
      }
      else {
        $this->setMessage('dpgit', 'clone', 'Git: Failed to clone !url to !dir:<br/>@out', array(
          '!url' => $this->conf('url'),
          '!dir' => $wdir,
          '@out' => join($out, '<br/>'),
        ), WATCHDOG_ERROR);
      }

      // Return command output if we got a $status_out reference
      if ($status_out) {
        $status_out = join($out, '<br/>');
      }
      return ($return === 0);
    }
    else {
      // Let people know if we couldn't prepare a parent directory for the working dir
      $this->setMessage('dpgit', 'clone', 'Git: Failed to create working directory, the parent directory !dir does not exist and could not be created', array(
        '!dir' => dirname($wdir),
      ), WATCHDOG_ERROR);
    }
  }

  /**
   * Callback that creates a temporary directory with the
   * files to package.
   *
   * @param DProjectRelease $release
   *  A release object
   * @return string
   *  A path to a directory containing the files to package
   */
  public function package($release) {
    $repo = $this->workingDirectory();
    $target = $this->workingDirectory($release);
    $metadata = $release->getMetadataObject('git', DPGitPackager::GIT_NAMESPACE);

    $out = array();
    $res = DPGitPackager::checkout($target, $repo . '/.git', $metadata->sha1, $out);
    if (!$res) {
      $this->setMessage('dpgit', 'packaging', 'Failed to check out !sha1 to the directory !dir', array(
        '!sha1' => $metadata->sha1,
        '!dir' => $target,
      ));
    }
    else {
      return $target;
    }
  }

  /**
   * Checks out a specific commit of a git repo into a directory
   *
   * @param string $target_dir
   *  The directory to check out the source into
   * @param string $git_dir
   *  The .git directory of the source
   * @param string $sha1
   *  The hash of the commit to check out
   * @param array(string) $out
   *  An array of lines output by git
   * @return bool
   *  Returns TRUE on a successful checkout, FALSE on failure
   */
  public static function checkout($target_dir, $git_dir, $sha1, &$out) {
    // Make sure that the target dir exists
    if (!file_exists($target_dir)) {
      mkdir($target_dir, 0777, TRUE);
    }
    $ret = 0;
    exec(DPGitPackager::gitShellCommand('checkout ' . $sha1 . ' -- ./', $git_dir, $target_dir), $out, $ret);
    return $ret==0;
  }

  /**
   * Gets the sha1 hash for a reference
   *
   * @param string $reference
   *  The reference (sha1, tag or branch)
   * @return bool
   *  Returns the sha1 hash if the reference was resolved, otherwise FALSE is returned
   */
  public function referenceSha1($reference) {
    $wd = $this->workingDirectory();
    $info = DPGitPackager::resolveReference($wd . '/.git', $reference);
    return $info ? $info['sha1'] : FALSE;
  }

  /**
   * Lists the available (remote) branches in a repository
   *
   * @param string $repository
   *  The path to the .git directory
   *
   * @return array(string) An array with the names of the branches
   */
  public static function branches($repository) {
    $branches = array();
    $out = shell_exec(DPGitPackager::gitShellCommand('branch -r --no-color', $repository));

    foreach (split("\n", $out) as $line) {
      if (!empty($line)) {
        $branches[] = trim($line);
      }
    }

    return $branches;
  }

  /**
   * Lists the available tags in a repository
   *
   * @param string $repository
   *  The path to the .git directory
   *
   * @return array(string) An array with the names of the tags
   */
  public static function tags($repository) {
    $tags = array();
    $out = shell_exec(DPGitPackager::gitShellCommand('tag -l', $repository));
    foreach (split("\n", $out) as $line) {
      if (!empty($line)) {
        $tags[] = trim($line);
      }
    }

    return $tags;
  }

  /**
   * Returns information about a reference
   *
   * @param string $repository
   *  The path to the .git directory
   * @param string $reference
   *  The reference (sha1, tag or branch)
   * @return array
   *  Log entry for the reference
   */
  public static function resolveReference($repository, $reference) {
    $l = DPGitPackager::log($repository, 1, $reference);
    foreach ($l as $sha1 => $info) {
      if (!empty($sha1)) {
        $info['sha1'] = $sha1;
        return $info;
      }
      break;
    }
    return FALSE;
  }

  /**
   * Gets the commit log as a associative arrayed keyed to the sha1 of the commit
   *
   * Structure:
   * array(
   *  [sha1] => array(
   *    'header' => array(
   *      'author'  => [name <email>],
   *      'date'    => [iso date-time],
   *    ),
   *    'message' => [commit message],
   *    'files' => array(
   *      [file]  => [status],
   *      ...
   *    )
   *  ),
   *  ...
   * )
   *
   * @param string $repository
   *  The path to the .git directory
   * @param int $commits
   *  The number of log entries to return. Use 0 to get the full log.
   *  Defaults to 10.
   * @param string $sha1
   *  Where to start the log from, defaults to HEAD
   * @param string $file
   *  The path to get the log for (relative to the repository root)
   *  Defaults to '.'
   * @param bool $raw
   *  Set to TRUE to skip parsing and return the raw log
   *
   * @return array
   *  The commit log as a associative arrayed keyed to the sha1 of the commit.
   *  Or the lines in the log in a array if $raw is set to TRUE.
   */
  public static function log($repository, $commits=10, $sha1='HEAD', $file='.', $raw=FALSE) {
    $out = array();
    $retval = 0;

    // Create a -<n> flag if we should limit the number of log messages to fetch
    $n_flag = '';
    if ($commits) {
      $n_flag = '-' . $commits;
    }

    // We can't use exec as it doesn't preserve trailing whitespace and we need
    // that to differentiate between empty lines in the commit messages and empty
    // lines separating header, commit message and file listing.
    $out = shell_exec(DPGitPackager::gitShellCommand('log ' . $n_flag . ' --no-color --date=iso --name-status ' . $sha1 . ' -- .', $repository));

    $out = split("\n", $out);

    // Return the raw output if requested
    if ($raw) {
      return $out;
    }

    $log = array();
    $lines = count($out);
    for ($i=0; $i<$lines; $i++) {
      $match = array();
      if (preg_match('/commit ([0-9a-f]+)/', $out[$i], $match)) {
        $sha = $match[1];
      }
      else { // Skip forward if we don't get a sha
        continue;
      }

      $info = array();

      // Parse the header
      $i++;
      $match = array();
      $info['header'] = array();
      while (!empty($out[$i]) && preg_match('/^([^:]+):\s*(.+)$/', $out[$i], $match)) {
        $info['header'][drupal_strtolower($match[1])] = $match[2];
        $i++;
      }

      // Extract the commit message
      $i++;
      $message = '';
      while (!empty($out[$i])) {
        $message .= trim($out[$i]) . "\n";
        $i++;
      }
      $info['message'] = $message;

      // Get info about changes
      $i++;
      $files = array();
      while (!empty($out[$i])) {
        $files[drupal_substr($out[$i], 2)] = drupal_substr($out[$i], 0, 1);
        $i++;
      }
      $info['files'] = $files;

      $log[$sha] = $info;
    }

    return $log;
  }

  /**
   * Fetches updates from the origin repositiory
   *
   * @param string $status_out
   * @return void
   */
  public function update(&$status_out=NULL) {
    $wdir = $this->workingDirectory();
    if (file_exists($wdir)) {
      $out = array();
      $return = 0;
      $cmd = DPGitPackager::gitShellCommand('fetch origin', $wdir . '/.git');
      exec($cmd, $out, $return);

      if ($return === 0) {
        drupal_set_message(t('Git: Fetched updates from !url', array('!url' => $this->conf('url'))));
      }
      else {
        $this->setMessage('dpgit', 'fetch', 'Git: Failed to fetch updates from !url to !dir:<br/>@out', array(
          '!url' => $this->conf('url'),
          '!dir' => $wdir,
          '@out' => join($out, '<br/>'),
        ), WATCHDOG_ERROR);
      }

      // Return command output if we got a $status_out reference
      if (is_string($status_out)) {
        $status_out = join($out, '<br/>');
      }
      return ($return === 0);
    }
    else {
      $this->setMessage('dpgit', 'fetch', 'Git: The working directory !dir doesn\'t exist', array(
        '!dir' => $wdir,
      ), WATCHDOG_ERROR);
      return FALSE;
    }
  }

  /**
   * Helper function that creates shell commands for working with git
   *
   * @param string $command
   *  The command that should be sent to git
   * @param string $git_dir
   *  Optional. The path to the .git directory that should be used
   * @param string $work_dir
   *  Optional. The path to the working directory that should be used
   * @return string
   *  The full command, including the path to the git binary, and options
   *  for git and working directory
   */
  public static function gitShellCommand($command, $git_dir=NULL, $work_dir=NULL) {
    if ($git_dir) {
      $command = '--git-dir="' . realpath($git_dir) . '" ' . $command;
    }
    if ($work_dir) {
      $command = '--work-tree="' . $work_dir . '" ' . $command;
    }

    $git_path = variable_get('git_path', 'git');
    return $git_path . ' ' . $command;
  }

  /**
   * Expands the Id keywords in a file
   *
   * @param DProjectRelease $release
   *  The release that this file is part of
   * @param array $path_info
   *  Path information for the file
   * @param string $content
   *  The contents of the file
   * @return void
   */
  public static function expandKeywords(&$release, $path_info, &$content) {
    // The supported file extensions
    $ext = array(
      'module' => TRUE,   'inc' => TRUE,
      'install' => TRUE,  'js' => TRUE,
      'info' => TRUE,     'css' => TRUE,
      'php' => TRUE,
    );
    if (isset($ext[$path_info['extension']])) {
      $metadata = $release->getMetadataObject('git', DPGitPackager::GIT_NAMESPACE);
      $id_kw = '$Id: ' . $path_info['basename'] . ', sha1: ' . $metadata->sha1 . ' ' .
        date('c', strtotime($metadata->info->header->date)) . ' ' . $metadata->info->header->author . ' Exp $';
      $count = 0;
      $content = preg_replace('/\$Id\$/', $id_kw, $content, -1, $count);
    }
  }

  /**
   * Callback that is used to get tags for
   * the packager source list.
   *
   * @return array
   */
  public function tagsOptions() {
    $wdir = $this->workingDirectory();
    $options = array();
    foreach (DPGitPackager::tags($wdir . '/.git') as $tag) {
      $options['tags/' . $tag] = $tag;
    }
    return $options;
  }

  /**
   * Callback that is used to get branches for
   * the packager source list.
   *
   * @return array
   */
  public function branchesOptions() {
    $wdir = $this->workingDirectory();
    $options = array();
    foreach (DPGitPackager::branches($wdir . '/.git') as $branch) {
      $options['refs/remotes/' . $branch] = $branch;
    }
    return $options;
  }

  /**
   * Callback that is used to get the recent commits for
   * the packager source list.
   *
   * @return array
   */
  public function recentCommitsOptions() {
    $wdir = $this->workingDirectory();
    $log = DPGitPackager::log($wdir . '/.git', 20, 'origin/master');

    $recent_commits = array();
    foreach ($log as $sha1 => $info) {
      $time = strtotime($info['header']['date']);
      $recent_commits[$sha1] = format_date($time) . ' - ' . $sha1;
    }

    return $recent_commits;
  }
}