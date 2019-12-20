package br.ufscar.dc.ppgcc.gsdr.minas.datasets.kdd

object Kdd {
  type continuous = Double
  type symbolic = String

  val sampleLine = "0,tcp,http,SF,215,45076,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,1,1,0.00,0.00,0.00,0.00,1.00,0.00,0.00,0,0,0.00,0.00,0.00,0.00,0.00,0.00,0.00,0.00,normal."
  lazy val sampleLineList = sampleLine.split(",")
  val symbolicIndexes = Map[Int, String](
    2  -> "protocol_type",
    3  -> "service",
    4  -> "flag",
    7  -> "land",
    12 -> "logged_in",
    21 -> "is_host_login",
    22 -> "is_guest_login",
    42 -> "label"
  ).map(x => (x._1 - 1 -> x._2))
  val symbolicDictionaries = Map[String, Set[String]](
    "label" ->          Set(
      "buffer_overflow.", "satan.", "loadmodule.", "smurf.", "multihop.",
      "warezmaster.", "portsweep.", "perl.", "ftp_write.", "warezclient.", "land.", "nmap.", "neptune.",
      "spy.", "guess_passwd.", "pod.", "normal.", "phf.", "rootkit.", "imap.", "ipsweep.", "teardrop.", "back."
    ),
    "service" ->        Set(
      "ftp", "netbios_ssn", "hostnames", "printer", "finger", "smtp", "harvest", "aol", "name", "whois",
      "http_8001", "private", "sql_net", "shell", "ftp_data", "auth", "ssh", "telnet", "gopher", "pop_2", "domain",
      "pm_dump", "supdup", "netbios_dgm", "discard", "nnsp", "X11", "ctf", "red_i", "tim_i", "csnet_ns", "sunrpc",
      "urh_i", "Z39_50", "tftp_u", "remote_job", "domain_u", "courier", "http_443", "iso_tsap", "efs", "nntp", "link",
      "daytime", "bgp", "ecr_i", "echo", "mtp", "uucp_path", "eco_i", "pop_3", "http_2784", "klogin", "ntp_u",
      "netstat", "systat", "ldap", "time", "login", "uucp", "vmnet", "urp_i", "exec", "http", "netbios_ns", "other",
      "imap4", "rje", "kshell", "IRC"
    ),
    "is_guest_login" -> Set("0", "1"),
    "flag" ->           Set("S1", "RSTR", "REJ", "S0", "RSTOS0", "OTH", "SH", "S3", "SF", "RSTO", "S2"),
    "land" ->           Set("0", "1"),
    "protocol_type" ->  Set("tcp", "udp", "icmp"),
    "logged_in" ->      Set("0", "1"),
    "is_host_login" ->  Set("0", "1")
  )
  val symbolicIndex = symbolicIndexes.keySet
  val indexMap = symbolicIndexes.mapValues(x => symbolicDictionaries(x).toIndexedSeq)
  val dimension = 41
  val magnitude = 4898431
  val magnitudeOne10th = 494021
  val classes = 22
  lazy val dimension_ = sampleLineList.size - 1
  lazy val classes_ = symbolicDictionaries("label").size

  /**
   * stream lines in the form
   * "0,tcp,http,SF,215,45076,0,0,0,0,0,1,
   *    0,0,0,0,0,0,0,0,0,0,1,1,0.00,0.00,0.00,0.00,1.00,0.00,0.00,0,0,0.00,0.00,0.00,0.00,0.00,0.00,0.00,0.00,normal."
   * duration, protocol_type, service, flag, src_bytes, dst_bytes, land, wrong_fragment, urgent, hot, num_failed_logins, logged_in,
   *    num_compromised, root_shell, su_attempted, num_root, num_file_creations, num_shells, num_access_files, num_outbound_cmds,
   *    is_host_login, is_guest_login, count, srv_count, serror_rate, srv_serror_rate, rerror_rate, srv_rerror_rate, same_srv_rate,
   *    diff_srv_rate, srv_diff_host_rate, dst_host_count, dst_host_srv_count, dst_host_same_srv_rate, dst_host_diff_srv_rate,
   *    dst_host_same_src_port_rate, dst_host_srv_diff_host_rate, dst_host_serror_rate, dst_host_srv_serror_rate, dst_host_rerror_rate,
   *    dst_host_srv_rerror_rate
   * 1    duration: continuous.
   * 2    protocol_type: symbolic.
   * 3    service: symbolic.
   * 4    flag: symbolic.
   * 5    src_bytes: continuous.
   * 6    dst_bytes: continuous.
   * 7    land: symbolic.
   * 8    wrong_fragment: continuous.
   * 9    urgent: continuous.
   * 10   hot: continuous.
   * 11   num_failed_logins: continuous.
   * 12   logged_in: symbolic.
   * 13   num_compromised: continuous.
   * 14   root_shell: continuous.
   * 15   su_attempted: continuous.
   * 16   num_root: continuous.
   * 17   num_file_creations: continuous.
   * 18   num_shells: continuous.
   * 19   num_access_files: continuous.
   * 20   num_outbound_cmds: continuous.
   * 21   is_host_login: symbolic.
   * 22   is_guest_login: symbolic.
   * 23   count: continuous.
   * 24   srv_count: continuous.
   * 25   serror_rate: continuous.
   * 26   srv_serror_rate: continuous.
   * 27   rerror_rate: continuous.
   * 28   srv_rerror_rate: continuous.
   * 29   same_srv_rate: continuous.
   * 30   diff_srv_rate: continuous.
   * 31   srv_diff_host_rate: continuous.
   * 32   dst_host_count: continuous.
   * 33   dst_host_srv_count: continuous.
   * 34   dst_host_same_srv_rate: continuous.
   * 35   dst_host_diff_srv_rate: continuous.
   * 36   dst_host_same_src_port_rate: continuous.
   * 37   dst_host_srv_diff_host_rate: continuous.
   * 38   dst_host_serror_rate: continuous.
   * 39   dst_host_srv_serror_rate: continuous.
   * 40   dst_host_rerror_rate: continuous.
   * 41   dst_host_srv_rerror_rate: continuous.
   * 42   label: symbolic.
   */
  case class KddEntry(
       duration: Double, protocol_type: String, service: String, flag: String,
       src_bytes: Double, dst_bytes: Double,
       land: String, wrong_fragment: Double, urgent: Double, hot: Double,
       num_failed_logins: Double, logged_in: String,
       num_compromised: Double, root_shell: Double, su_attempted: Double,
       num_root: Double, num_file_creations: Double,
       num_shells: Double, num_access_files: Double, num_outbound_cmds: Double,
       is_host_login: String, is_guest_login: String,
       count: Double, srv_count: Double, serror_rate: Double, srv_serror_rate: Double,
       rerror_rate: Double,
       srv_rerror_rate: Double, same_srv_rate: Double, diff_srv_rate: Double,
       srv_diff_host_rate: Double, dst_host_count: Double,
       dst_host_srv_count: Double, dst_host_same_srv_rate: Double, dst_host_diff_srv_rate: Double,
       dst_host_same_src_port_rate: Double,
       dst_host_srv_diff_host_rate: Double, dst_host_serror_rate: Double, dst_host_srv_serror_rate: Double,
       dst_host_rerror_rate: Double,
       dst_host_srv_rerror_rate: Double, label: String = ""
     )
  case class ConversionFail(msg: List[String])
  type MaybeEntry = Either[KddEntry, ConversionFail]

  /**
   * stream lines in the form
   * "0,tcp,http,SF,215,45076,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,1,1,0.00,0.00,0.00,0.00,1.00,0.00,0.00,0,0,0.00,0.00,0.00,0.00,0.00,0.00,0.00,0.00,normal."
   *
   * @param line
   * @return
   */
  def fromLine(line: String): Seq[Double] = {
    val values = line.split(",")
    for {
      i <- 0 until values.length
    } yield if (symbolicIndex.contains(i)) indexMap(i).indexOf(values(i)).toDouble else values(i).toDouble
  }
}
