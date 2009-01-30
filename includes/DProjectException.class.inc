<?php

/**
 * A exception that allows localization of the error message
 * and a function for logging the exception to watchdog
 **/
class DProjectException extends Exception {
  private $tmessage;
  private $vars;
  private $logged = FALSE;

  /**
   * Constructor for DProjectException
   *
   * @param string $message
   *  The message to store in the log. See t() for documentation
   *  on how $message and $variables interact. Keep $message
   *  translatable by not concatenating dynamic values into it!
   * @param $variables
   *  Array of variables to replace in the message, defaults to empty array.
   * @param $code
   *  User defined exception code, defaults to 0.
   * @param $log
   *  Set to TRUE to immediately log the exception to watchdog, defaults to FALSE.
   *  Immediate logging will always use the severity level of WATCHDOG_ERROR.
   **/
  public function DProjectException($message, $variables=array(), $code=0, $log=FALSE) {
    parent::__construct(t($message, $variables), $code);

    $this->tmessage = $message;
    $this->vars = $vars;
    if ($log) {
      $this->log();
    }
  }

  /**
   * Logs the exception to watchdog. This function call only has an effect the first time it's
   * called. Therefore you can safely call this function without worrying about double logging.
   *
   * @param $severity
   *  The severity of the message, as per RFC 3164
   * @param $link
   *  A link to associate with the message.
   *
   * @see watchdog()
   * @see watchdog_severity_levels()
   **/
  public function log($severity=WATCHDOG_ERROR, $link=NULL) {
    if (!$logged) {
      watchdog('dproject', $this->message, $this->vars, $severity, $link);
      $logged = TRUE;
    }
    return $this;
  }
}