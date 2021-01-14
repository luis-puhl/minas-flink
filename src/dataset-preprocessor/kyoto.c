#include <stdio.h>
#include <stdio_ext.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/*
#Duration	#Service	#Source_bytes	#Destination_bytes	#Count	#Same_srv_rate	#Serror_rate	#Srv_serror_rate	#Dst_host_count	#Dst_host_srv_count	#Dst_host_same_src_port_rate	#Dst_host_serror_rate	#Dst_host_srv_serror_rate	#Flag	#IDS_detection	#Malware_detection	#Ashula_detection	#Label	#Source_IP_Address	#Source_Port_Number	#Destination_IP_Address	#Destination_Port_Number	#Start_Time	#protocol
0.000000	other	0	0	0	0.00	0.00	0.00	0	0	0.00	0.00	0.00	S0	0	0	0	-1	fdbd:f115:35b2:0424:40aa:098c:03e5:149b	3712	fdbd:f115:35b2:c891:7db9:2762:6182:03eb	445	00:00:00	tcp
2.954396	other	0	0	0	0.00	0.00	0.00	0	0	0.00	0.00	0.00	S0	0	0	0	-1	fdbd:f115:35b2:0424:40aa:098c:03e5:149b	3924	fdbd:f115:35b2:c891:7db9:2762:6182:03eb	445	00:00:01	tcp
0.000000	other	0	0	0	0.00	0.00	0.00	0	0	0.00	0.00	0.00	S0	0	0	0	-1	fdbd:f115:35b2:4bc1:5d3d:0b94:0ad3:05e7	46484	fdbd:f115:35b2:ca03:7da1:271d:60bf:0092	3128	00:00:02	tcp
*/

void handleFloat(char *value, float limit) {
    /*
    d = pd.DataFrame({ str(i) : dd.quantile(i) for i in [0.1, 0.5, 0.75, 0.9, 0.95, 0.99, 0.999]})
    |                              |   0.1 |     0.5 |     0.75 |       0.9 |       0.95 |       0.99 |      0.999 |
    |:-----------------------------|------:|--------:|---------:|----------:|-----------:|-----------:|-----------:|
    | #Duration                    |     0 | 5.8e-05 |  1.89942 |   3.00427 |    3.29681 |    7.67176 |    50.1231 |
    | #Source_bytes                |     0 | 0       |  3       |  65       |  605       | 1367       |  4491.4    |
    | #Destination_bytes           |     0 | 0       |  0       | 188       | 2605       | 2937       | 27126      |
    | #Count                       |     0 | 0       |  1       |   3       |    9       |   33       |    95      |
    | #Same_srv_rate               |     0 | 0       |  1       |   1       |    1       |    1       |     1      |
    | #Serror_rate                 |     0 | 0       |  0       |   0       |    1       |    1       |     1      |
    | #Srv_serror_rate             |     0 | 0.75    |  1       |   1       |    1       |    1       |     1      |
    | #Dst_host_count              |     0 | 0       |  2       |  88       |   98       |  100       |   100      |
    | #Dst_host_srv_count          |     0 | 1       | 43       |  96       |   98       |  100       |   100      |
    | #Dst_host_same_src_port_rate |     0 | 0       |  0       |   0.33    |    1       |    1       |     1      |
    | #Dst_host_serror_rate        |     0 | 0       |  0       |   0.92    |    1       |    1       |     1      |
    | #Dst_host_srv_serror_rate    |     0 | 0       |  0       |   1       |    1       |    1       |     1      |
    */
    // 2.954396
    float f;
    sscanf(value, "%e", &f);
    if (f > limit) {
        f = limit;
    }
    f /= limit;
    printf("%e,", f);
}

void handleIPAddr(char *value) {
    // fdbd:f115:35b2:4bc1:5d3d:0b94:0ad3:05e7
    int octet[8];
    int assigned = sscanf(value, "%4x:%4x:%4x:%4x:%4x:%4x:%4x:%4x", &octet[0], &octet[1], &octet[2], &octet[3], &octet[4], &octet[5], &octet[6], &octet[7]);
    if (assigned != 8) exit(-1);
    for (size_t i = 0; i < 8; i++) {
        printf("%e,", ((float) octet[i]) / ((float) 0xffff));
    }
}
void handlePort(char *value) {
    // 3712
    int v;
    sscanf(value, "%d", &v);
    printf("%e,", ((float) v) / ((float) 65535));
}
void handleTime(char *value) {
    // 23:59:58
    int hour, min, sec;
    sscanf(value, "%2d:%2d:%2d", &hour, &min, &sec);
    float val = sec + 60 * (min + (60 * hour));
    float max = 60 + 60 * (60 + (60 * 60));
    printf("%e,", val / max);
}
void handleConnectionState(char *value) {
    // '#Flag': array(['OTH', 'REJ', 'RSTO', 'RSTOS0', 'RSTR', 'RSTRH', 'S0', 'S1', 'S2', 'S3', 'SF', 'SH', 'SHR'], dtype=object)
    //                   0,      1,     2,       3,      4,      5,      6,       7,  8,      9,  10,    11,   12
    int state = 0;
    if (value[0] == 'O') {
        // OTH
        state = 0;
    } else if (value[0] == 'R') {
        // 'REJ', 'RSTO', 'RSTOS0', 'RSTR', 'RSTRH',
        state = 1;
        if (value[1] != 'E') {
            // 'RSTO', 'RSTOS0', 'RSTR', 'RSTRH',
            if (value[3] == 'O') {
                state = value[4] == '\0' ? 2 : 3;
            } else if (value[3] == 'R') {
                state = value[4] == '\0' ? 5 : 5;
            }
        }
    } else if (value[0] == 'S') {
        // 'S0', 'S1', 'S2', 'S3', 'SF', 'SH', 'SHR'
        if (value[2] >= '0' && value[2] <= '3') {
            state = 6 + (value[2] - '0');
        } else {
            state = value[2] == 'F' ? 10 : 11;
            state += value[3] == 'R';
        }
    }
    printf("%e,", ((float)state) / 12);
}

void handleField(char *value, int fieldCounter) {
    switch (fieldCounter) {
    case 0:
        // #Duration
        handleFloat(value, 3.0f);
        break;
    case 1:
        // #Service
        break;
    case 2:
        // #Source_bytes
        handleFloat(value, 65);
        break;
    case 3:
        // #Destination_bytes
        handleFloat(value, 188);
        break;
    case 4:
        // #Count
        handleFloat(value, 9);
        break;
    case 5:
        // #Same_srv_rate
        handleFloat(value, 1);
        break;
    case 6:
        // #Serror_rate
        handleFloat(value, 1);
        break;
    case 7:
        // #Srv_serror_rate
        handleFloat(value, 1);
        break;
    case 8:
        // #Dst_host_count
        handleFloat(value, 88);
        break;
    case 9:
        // #Dst_host_srv_count
        handleFloat(value, 96);
        break;
    case 10:
        // #Dst_host_same_src_port_rate
        handleFloat(value, 1);
        break;
    case 11:
        // #Dst_host_serror_rate
        handleFloat(value, 1);
        break;
    case 12:
        // #Dst_host_srv_serror_rate
        handleFloat(value, 1);
        break;
    case 13:
        // '#Flag'
        handleConnectionState(value);
        break;
    case 14:
        // #IDS_detection
        // printf("%e,", value[0] == '0' ? 0.0f : 1.0f);
        break;
    case 15:
        // #Malware_detection
        // printf("%e,", value[0] == '0' ? 0.0f : 1.0f);
        break;
    case 16:
        // #Ashula_detection
        // printf("%e,", value[0] == '0' ? 0.0f : 1.0f);
        break;
    case 17:
        // '#Label': array([-1,  1]),
        // printf("%e,", value[0] == '1' ? 0.0f : 1.0f);
        break;
    case 18:
        // #Source_IP_Address
        handleIPAddr(value);
        break;
    case 19:
        // #Source_Port_Number
        handlePort(value);
        break;
    case 20:
        // #Destination_IP_Address
        handleIPAddr(value);
        break;
    case 21:
        // #Destination_Port_Number
        handlePort(value);
        break;
    case 22:
        // #Start_Time
        handleTime(value);
        break;
    case 23:
        // #protocol array(['tcp', 'udp', 'icmp'],
        printf("%e,", ((float)value[0]) / 255);
        break;
    default:
        break;
    }
}

int main(int argc, char const *argv[]) {
    unsigned long lineSize = 0;
    char *line = NULL;
    char delim[] = "\t";
    int lineCounter = 0;

    for (
        int lineLen = getline(&line, &lineSize, stdin);
        lineLen > 0;
        lineLen = getline(&line, &lineSize, stdin)
    ) {
        char class = 'N';
        if (lineCounter == 0) {
            // printf("%s", line);
            printf("#Duration,Service,srcBytes,dstBytes,Count,"
            "SameSrvRate,SerrorRate,SrvSerrorRate,DstHostCount,"
            "DstHostSrvCount,DstHostSameSrcPortRate,DstHostSerrorRate,"
            "DstHostSrvSerrorRate,Flag,"
            "srcIP,ip1,ip2,ip3,ip4,ip5,ip6,ip7,srcPort,"
            "dstIP,ip1,ip2,ip3,ip4,ip5,ip6,ip7,dstPort,time,protocol,class\n");
        } else {
            int fieldCounter = 0;
            float f;
            for (char *value = strtok(line, delim); value != NULL; value = strtok(NULL, delim)) {
                handleField(value, fieldCounter);
                if (fieldCounter == 17 && value[0] != '1') {
                    class = 'A';
                }
                // printf("'%s'\n", value);
                fieldCounter++;
            }
            printf("%c\n", class);
        }
        lineCounter++;
    }
    free(line);
    return 0;
}
