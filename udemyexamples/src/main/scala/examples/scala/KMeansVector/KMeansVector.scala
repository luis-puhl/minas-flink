package examples.scala.KMeansVector

import org.apache.flink.api.common.typeinfo.{TypeHint, TypeInformation}
import org.apache.flink.streaming.api.scala.{DataStream, StreamExecutionEnvironment}

import scala.collection.immutable

object KMeansVector extends App {
  val env = StreamExecutionEnvironment.getExecutionEnvironment
  val kdd$: DataStream[String] = env.socketTextStream("localhost", 3232)
  implicit val KddEntryType = TypeInformation.of(new TypeHint[Kdd.KddEntry] {})
  val csvTuple$: DataStream[Kdd.KddEntry] = kdd$.map { line => Kdd.fromLine(line) }

}


object Kdd {
  type continuous = Long
  type symbolic = String

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
   * duration: continuous.
   * protocol_type: symbolic.
   * service: symbolic.
   * flag: symbolic.
   * src_bytes: continuous.
   * dst_bytes: continuous.
   * land: symbolic.
   * wrong_fragment: continuous.
   * urgent: continuous.
   * hot: continuous.
   * num_failed_logins: continuous.
   * logged_in: symbolic.
   * num_compromised: continuous.
   * root_shell: continuous.
   * su_attempted: continuous.
   * num_root: continuous.
   * num_file_creations: continuous.
   * num_shells: continuous.
   * num_access_files: continuous.
   * num_outbound_cmds: continuous.
   * is_host_login: symbolic.
   * is_guest_login: symbolic.
   * count: continuous.
   * srv_count: continuous.
   * serror_rate: continuous.
   * srv_serror_rate: continuous.
   * rerror_rate: continuous.
   * srv_rerror_rate: continuous.
   * same_srv_rate: continuous.
   * diff_srv_rate: continuous.
   * srv_diff_host_rate: continuous.
   * dst_host_count: continuous.
   * dst_host_srv_count: continuous.
   * dst_host_same_srv_rate: continuous.
   * dst_host_diff_srv_rate: continuous.
   * dst_host_same_src_port_rate: continuous.
   * dst_host_srv_diff_host_rate: continuous.
   * dst_host_serror_rate: continuous.
   * dst_host_srv_serror_rate: continuous.
   * dst_host_rerror_rate: continuous.
   * dst_host_srv_rerror_rate: continuous.
   */
  case class KddEntry(
       duration: continuous, protocol_type: symbolic, service: symbolic, flag: symbolic,
       src_bytes: continuous, dst_bytes: continuous,
       land: symbolic, wrong_fragment: continuous, urgent: continuous, hot: continuous,
       num_failed_logins: continuous, logged_in: symbolic,
       num_compromised: continuous, root_shell: continuous, su_attempted: continuous,
       num_root: continuous, num_file_creations: continuous,
       num_shells: continuous, num_access_files: continuous, num_outbound_cmds: continuous,
       is_host_login: symbolic, is_guest_login: symbolic,
       count: continuous, srv_count: continuous, serror_rate: continuous, srv_serror_rate: continuous,
       rerror_rate: continuous,
       srv_rerror_rate: continuous, same_srv_rate: continuous, diff_srv_rate: continuous,
       srv_diff_host_rate: continuous, dst_host_count: continuous,
       dst_host_srv_count: continuous, dst_host_same_srv_rate: continuous, dst_host_diff_srv_rate: continuous,
       dst_host_same_src_port_rate: continuous,
       dst_host_srv_diff_host_rate: continuous, dst_host_serror_rate: continuous, dst_host_srv_serror_rate: continuous,
       dst_host_rerror_rate: continuous,
       dst_host_srv_rerror_rate: continuous, label: symbolic = ""
     )
  /**
   * stream lines in the form
   * "0,tcp,http,SF,215,45076,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,1,1,0.00,0.00,0.00,0.00,1.00,0.00,0.00,0,0,0.00,0.00,0.00,0.00,0.00,0.00,0.00,0.00,normal."
   *
   * @param line
   * @return
   */
  def fromLine(line: String): KddEntry =
    fromList(line.split(",").toList)
  def fromList(args: List[String]): KddEntry =
    if (args.length != 42) throw new RuntimeException("Invalid arguments length")
    else KddEntry(
        duration =                    args(1).toLong,
        protocol_type =               args(2),
        service =                     args(3),
        flag =                        args(4),
        src_bytes =                   args(5).toLong,
        dst_bytes =                   args(6).toLong,
        land =                        args(7),
        wrong_fragment =              args(8).toLong,
        urgent =                      args(9).toLong,
        hot =                         args(10).toLong,
        num_failed_logins =           args(11).toLong,
        logged_in =                   args(12),
        num_compromised =             args(13).toLong,
        root_shell =                  args(14).toLong,
        su_attempted =                args(15).toLong,
        num_root =                    args(16).toLong,
        num_file_creations =          args(17).toLong,
        num_shells =                  args(18).toLong,
        num_access_files =            args(19).toLong,
        num_outbound_cmds =           args(20).toLong,
        is_host_login =               args(21),
        is_guest_login =              args(22),
        count =                       args(23).toLong,
        srv_count =                   args(24).toLong,
        serror_rate =                 args(25).toLong,
        srv_serror_rate =             args(26).toLong,
        rerror_rate =                 args(27).toLong,
        srv_rerror_rate =             args(28).toLong,
        same_srv_rate =               args(29).toLong,
        diff_srv_rate =               args(30).toLong,
        srv_diff_host_rate =          args(31).toLong,
        dst_host_count =              args(32).toLong,
        dst_host_srv_count =          args(33).toLong,
        dst_host_same_srv_rate =      args(34).toLong,
        dst_host_diff_srv_rate =      args(35).toLong,
        dst_host_same_src_port_rate = args(36).toLong,
        dst_host_srv_diff_host_rate = args(37).toLong,
        dst_host_serror_rate =        args(38).toLong,
        dst_host_srv_serror_rate =    args(39).toLong,
        dst_host_rerror_rate =        args(40).toLong,
        dst_host_srv_rerror_rate =    args(41).toLong,
        label =                       args(42)
    )
}
