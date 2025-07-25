
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>
#include <stdbool.h>

static void usage(char *progname)
{
    printf("%s [-h] \n", progname);
    printf("  -h : 帮助 \n");
    printf("  -p : 检查系统是否联网 \n");
    printf("  -g : 获得当前系统使用的网卡 \n");
    printf("  -b : 4G网卡拨号 \n");
    printf("    %s -b up : 4G网卡拨号 \n", progname);
    printf("    %s -b down : 4G网卡断开拨号 \n", progname);
    printf("  -f : 共享网络 \n");
    printf("    %s -f start <ifname> : 开始共享网络 \n", progname);
    printf("    %s -f stop <ifname> : 停止共享网络 \n", progname);
    printf("  -i : 设置当前使用的wifi网卡 \n");
    printf("    %s -i <ifname> \n", progname);
    printf("  -t : 获得当前网卡的状态 \n");
    printf("  -s : 扫描热点 \n");
    printf("  -l : 获得扫描的热点列表，在扫描热点几秒后调用 \n");
    printf("  -a : 添加一个wifi网络 \n");
    printf("    %s -a <ssid> <password>\n", progname);
    printf("  -c : 连接到一个wifi网络 \n");
    printf("    %s -c <id>\n", progname);
    printf("  -d : 获得当前网卡的IP地址，在网卡状态变为COMPLETED后调用 \n");
    printf("  -e : 获得已保存的wifi网络列表 \n");
    printf("  -r : 删除一个wifi网络 \n");
    printf("    %s -r <id>\n", progname);
}

/*
    * 检查系统是否联网
    * 返回值：0表示联网，-1表示未联网，-2表示出错
*/
int is_online() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return -2;
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family=AF_INET;
    serv_addr.sin_port=htons(80);
    serv_addr.sin_addr.s_addr = inet_addr("157.148.69.186"); 
    
    // 设置超时(1秒)
    struct timeval timeout = {.tv_sec = 1, .tv_usec = 0};
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    
    int conn = connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    close(sock);
    
    return (conn == 0) ? -1 : 0;
}

/*
    * 获得当前系统使用的网卡
    * 返回值：0表示成功，-1表示失败
    * buffer: 存储网卡名称的缓冲区
    * len: 缓冲区长度
*/
int get_network_scard(char *buffer, int len) {
    FILE *fp = popen("route -n | awk '$1 == \"0.0.0.0\" {print $8; exit}'", "r");
    if (!fp) {
        perror("popen() failed");
        return -1;
    }

    if(fgets(buffer, len, fp) != NULL) {
    }

    pclose(fp);

    return 0;
}

struct wifi_status {
    char interface[32];     // 当前使用的wifi网卡名称
    char bssid[18];         // 当前连接的BSSID
    int frequency;          // 当前连接的频率
    char ssid[64];          // 当前连接的SSID
    int id;                 // 当前连接的网络ID
    char mode[16];          // 当前连接的模式
    char wpa_state[32];     // 当前WPA状态
    char ip_address[16];    // 当前网卡IP地址
    char mac_address[18];   // 当前网卡MAC地址
};

/*
    * 获取wifi网卡状态
    * 返回值：0表示成功，-1表示失败
    * status: 存储wifi状态的结构体
*/
int get_wifi_interface_status(struct wifi_status *status) {
    char buffer[256];
    int ret = -1;
    FILE *fp = NULL;

    fp = popen("wpa_cli status", "r");
    if (!fp) {
        perror("popen() failed");
        ret = -1;
        goto error;
    }

    while (fgets(buffer, 256, fp) != NULL) {
        if (strstr(buffer, "Selected interface") != NULL) {
            sscanf(buffer, "Selected interface '%s'", status->interface);
            status->interface[strlen(status->interface) - 1] = '\0';
        } else if (strstr(buffer, "bssid=") != NULL) {
            sscanf(buffer, "bssid=%17s", status->bssid);
        } else if (strstr(buffer, "frequency=") != NULL) {
            sscanf(buffer, "frequency=%d", &status->frequency);
        } else if (strstr(buffer, "ssid=") != NULL) {
            sscanf(buffer, "ssid=%63s", status->ssid);
        } else if (strstr(buffer, "id=") != NULL) {
            sscanf(buffer, "id=%d", &status->id);
        } else if (strstr(buffer, "mode=") != NULL) {
            sscanf(buffer, "mode=%15s", status->mode);
        } else if (strstr(buffer, "wpa_state=") != NULL) {
            sscanf(buffer, "wpa_state=%31s", status->wpa_state);
        } else if (strstr(buffer, "ip_address=") != NULL) {
            sscanf(buffer, "ip_address=%15s", status->ip_address);
        } else if (strstr(buffer, "mac_address=") != NULL) {
            sscanf(buffer, "mac_address=%17s", status->mac_address);
        }
    }

    pclose(fp);
    fp = NULL;

    ret = 0;

error:
    if (fp != NULL) {
        pclose(fp);
        fp = NULL;
    }
    return ret;
}

/*
    * 选择wifi网卡
    * 返回值：0表示成功，-1表示失败
    * scard: 要选择的网卡名称
*/
int select_wifi_interface(char * scard)
{
    int ret = 0;
    char buffer[256];

    snprintf(buffer, sizeof(buffer), "wpa_cli interface %s", scard);
    FILE *fp = popen(buffer, "r");
    if (!fp) {
        perror("popen() failed");
        ret = -1;
        goto error;
    }

    if (fgets(buffer, 256, fp) != NULL) {
        char *ptr = strstr(buffer, scard);
        if (ptr != NULL) {
            ret = 0;
        } else {
            ret = -1;
        }
    } else {
        ret = -1;
    }

    pclose(fp);
    fp = NULL;

    ret = 0;

error:
    if (fp != NULL) {
        pclose(fp);
        fp = NULL;
    }
    return ret;
}

/*
    * 扫描wifi网络
    * 返回值：0表示成功，-1表示失败
*/
int scan_wifi_networks() {
    int ret = -1;
    FILE *fp = NULL;
    char buffer[256];

    fp = popen("wpa_cli scan", "r");
    if (!fp) {
        perror("popen() failed");
        ret = -1;
        goto error;
    }

    while (fgets(buffer, 256, fp) != NULL) {
        char *ptr = strstr(buffer, "OK");
        if (ptr != NULL) {
            ret = 0;
        } else {
            ret = -1;
        }
    }

    pclose(fp);
    fp = NULL;

error:
    if (fp != NULL) {
        pclose(fp);
        fp = NULL;
    }
    return ret;
}

struct wifi_node {
    char ssid[64];      // WiFi SSID
    char bssid[18];     // WiFi BSSID
    int signal_level;   // 信号强度
    int frequency;      // 频率
};

/*
    * 获取wifi网络列表
    * 返回值：0表示成功，-1表示失败
    * networks: 存储wifi网络的数组
    * max_networks: 数组的最大长度
    * networks_count: 实际获取到的网络数量
*/
int get_wifi_networks(struct wifi_node *networks, int max_networks, int *networks_count) {
    int ret = 0;
    FILE *fp = NULL;
    char buffer[256];
    int node_count = 0;

    fp = popen("wpa_cli scan_results", "r");
    if (!fp) {
        perror("popen() failed");
        ret = -1;
        goto error;
    }

    fgets(buffer, 256, fp);     // 跳过前面2行
    fgets(buffer, 256, fp);
    while (fgets(buffer, 256, fp) != NULL) {
        int count = 0;
        char *token = strtok(buffer, "	");
        while (token != NULL) {
            if (count == 0) {
                strncpy(networks[node_count].bssid, token, sizeof(networks[node_count].bssid) - 1);
                networks[node_count].bssid[sizeof(networks[node_count].bssid) - 1] = '\0';
            } else if (count == 1) {
                networks[node_count].frequency = atoi(token);
            } else if (count == 2) {
                networks[node_count].signal_level = atoi(token);
            } else if (count == 4) {
                strncpy(networks[node_count].ssid, token, sizeof(networks[node_count].ssid) - 1);
                networks[node_count].ssid[strlen(networks[node_count].ssid) - 1] = '\0';
                break;
            }
            token = strtok(NULL, "	");
            count ++;
        }

        if(strlen(networks[node_count].ssid) > 0) {
            node_count ++;
            if(node_count >= max_networks) {
                break;
            }
        }
    }

    if(node_count > 0) {
        ret = 0;
    } else {   
        ret = -1;
    }
    *networks_count = node_count;

    pclose(fp);
    fp = NULL;

error:
    if (fp != NULL) {
        pclose(fp);
        fp = NULL;
    }
    return ret;
}

/*
    * 添加一个wifi网络，添加后需要调用connect_wifi_networks()连接网络
    * 返回值：0表示成功，-1表示失败
    * ssid: WiFi SSID
    * password: WiFi密码
*/
int add_wifi_networks(char *ssid, char *password) {
    int ret = 0;
    int net_id = 0;
    char buffer[256];
    FILE *fp = NULL;

    // 添加网络
    fp = popen("wpa_cli add_network", "r");
    if (!fp) {
        perror("popen() failed");
        ret = -1;
        goto error;
    }

    fgets(buffer, 256, fp);     // 跳过前面1行
    fgets(buffer, 256, fp);
    sscanf(buffer, "%d", &net_id);

    pclose(fp);
    fp = NULL;

    // 设置SSID
    snprintf(buffer, sizeof(buffer), "wpa_cli set_network %d ssid '\"%s\"'", net_id, ssid);
    fp = popen(buffer, "r");
    if (!fp) {
        perror("popen() failed");
        ret = -1;
        goto error;
    }

    fgets(buffer, 256, fp);     // 跳过前面1行
    fgets(buffer, 256, fp);
    if (strstr(buffer, "OK") == NULL) {
        ret = -1;
        goto error;
    }

    pclose(fp);
    fp = NULL;

    // 设置password
    snprintf(buffer, sizeof(buffer), "wpa_cli set_network %d psk '\"%s\"'", net_id, password);
    fp = popen(buffer, "r");
    if (!fp) {
        perror("popen() failed");
        ret = -1;
        goto error;
    }

    fgets(buffer, 256, fp);     // 跳过前面1行
    fgets(buffer, 256, fp);
    if (strstr(buffer, "OK") == NULL) {
        ret = -1;
        goto error;
    }

    pclose(fp);
    fp = NULL;

    // 保存配置
    fp = popen("wpa_cli save_config", "r");
    if (!fp) {
        perror("popen() failed");
        // ret = -1;
        // goto error;
    }   

    // fgets(buffer, 256, fp);     // 跳过前面1行
    // fgets(buffer, 256, fp);
    // if (strstr(buffer, "OK") == NULL) {
    //     ret = -1;
    //     goto error;
    // }

    pclose(fp);
    fp = NULL;

    ret = 0;

error:
    if (fp != NULL) {
        pclose(fp);
        fp = NULL;
    }
    return ret;
}

/*
    * 连接到一个wifi网络，当wifi状态为COMPLETED时，表示连接成功，然后调用get_wifi_ip()获取IP地址
    * 返回值：0表示成功，-1表示失败
    * net_id：网络id
*/
int connect_wifi_networks(int net_id) {
    int ret = 0;
    char buffer[256];
    FILE *fp = NULL;

    // 选择网络
    snprintf(buffer, sizeof(buffer), "wpa_cli select_network %d", net_id);
    fp = popen(buffer, "r");
    if (!fp) {
        perror("popen() failed");
        ret = -1;
        goto error;
    }

    fgets(buffer, 256, fp);     // 跳过前面1行
    fgets(buffer, 256, fp);
    if (strstr(buffer, "OK") == NULL) {
        ret = -1;
        goto error;
    }

    pclose(fp);
    fp = NULL;

    // 使能网络
    snprintf(buffer, sizeof(buffer), "wpa_cli enable_network %d", net_id);
    fp = popen(buffer, "r");
    if (!fp) {
        perror("popen() failed");
        ret = -1;
        goto error;
    }

    fgets(buffer, 256, fp);     // 跳过前面1行
    fgets(buffer, 256, fp);
    if (strstr(buffer, "OK") == NULL) {
        ret = -1;
        goto error;
    }

    pclose(fp);
    fp = NULL;

    ret = 0;

error:
    if (fp != NULL) {
        pclose(fp);
        fp = NULL;
    }
    return ret;
}

/*
    * 获取当前网卡的IP地址
    * 返回值：0表示成功，-1表示wifi未连接，-2表示获取IP地址失败
*/
int get_wifi_ip()
{
    char buffer[256];
    struct wifi_status status = {0};
    FILE *fp = NULL;
    int ret = -1;

    if (get_wifi_interface_status(&status) == 0) {
        if(status.ssid != NULL) {
            snprintf(buffer, sizeof(buffer), "udhcpc -i %s -n", status.interface);
            fp = popen(buffer, "r");
            if (!fp) {
                perror("popen() failed");
                ret = -2;
                goto error;
            }

            while (fgets(buffer, 256, fp) != NULL) {
                printf("%s", buffer);
                if (strstr(buffer, "failing") != NULL) {
                    ret = -2;
                    goto error;
                }
            }
            pclose(fp);
            fp = NULL;
        } else {
            ret = -1;
            goto error;
        }
    } else {
        ret = -1;
        goto error;
    }

    ret = 0;

error:
    if (fp != NULL) {
        pclose(fp);
        fp = NULL;
    }
    return ret;
}

struct saved_wifi_node {
    int id;         // 网络ID
    char ssid[64];  // WiFi SSID
    bool curr;      // 是否为当前网络
};

/*
    * 获取已保存的wifi网络列表
    * 返回值：0表示成功，-1表示失败
    * networks: 存储已保存的wifi网络的数组
    * max_networks: 数组的最大长度
    * networks_count: 实际获取到的网络数量
*/
int get_saved_wifi_networks(struct saved_wifi_node *networks, int max_networks, int *networks_count)
{
    char buffer[256];
    int node_count = 0;
    int ret = -1;

    FILE *fp = popen("wpa_cli list_networks", "r");
    if (!fp) {
        perror("popen() failed");
        ret = -1;
        goto error;
    }

    fgets(buffer, 256, fp);     // 跳过前面2行
    fgets(buffer, 256, fp);
    while (fgets(buffer, 256, fp) != NULL) {
        int count = 0;
        char *token = strtok(buffer, "	");
        while (token != NULL) {
            if (count == 0) {
                networks[node_count].id = atoi(token);
            } else if (count == 1) {
                strncpy(networks[node_count].ssid, token, sizeof(networks[node_count].ssid) - 1);
                networks[node_count].ssid[sizeof(networks[node_count].ssid) - 1] = '\0';
            } else if (count == 3) {
                if (strstr(token, "DISABLED") != NULL) {
                    networks[node_count].curr = false;
                } else if (strstr(token, "CURRENT") != NULL) {
                    networks[node_count].curr = true;
                }
            }
            token = strtok(NULL, "	");
            count ++;
        }
        node_count ++;
        if(node_count >= max_networks) {
            break;
        }
    }

    pclose(fp);
    fp = NULL;

    *networks_count = node_count;
    ret = 0;

error:
    if (fp != NULL) {
        pclose(fp);
        fp = NULL;
    }
    return ret;
}

/*
    * 删除一个wifi网络
    * 返回值：0表示成功，-1表示失败
    * id: 要删除的网络ID，-1表示删除当前网络
*/
int remove_wifi_networks(int id)
{
    int ret = 0;
    char buffer[256];
    FILE *fp = NULL;
    int net_id = id;

    // 获取当前网络状态
    struct wifi_status status = {0};
    if (get_wifi_interface_status(&status) != 0) {
        fprintf(stderr, "获取当前wifi状态失败\n");
        return -1;
    }

    if (id < 0) {
        net_id = status.id;
    }

    // 禁用网络
    snprintf(buffer, sizeof(buffer), "wpa_cli disable_network %d", net_id);
    fp = popen(buffer, "r");
    if (!fp) {
        perror("popen() failed");
        ret = -1;
        goto error;
    }

    fgets(buffer, 256, fp); 
    fgets(buffer, 256, fp);
    if (strstr(buffer, "OK") == NULL) {
        ret = -1;
        goto error;
    }

    pclose(fp);
    fp = NULL;

    // 删除网络
    snprintf(buffer, sizeof(buffer), "wpa_cli remove_network %d", net_id);
    fp = popen(buffer, "r");
    if (!fp) {
        perror("popen() failed");
        ret = -1;
        goto error;
    }

    fgets(buffer, 256, fp);     // 跳过前面1行
    fgets(buffer, 256, fp);
    if (strstr(buffer, "OK") == NULL) {
        ret = -1;
        goto error;
    }

    pclose(fp);
    fp = NULL;

    // 保存配置
    fp = popen("wpa_cli save_config", "r");
    if (!fp) {
        perror("popen() failed");
        // ret = -1;
        // goto error;
    }   

    // fgets(buffer, 256, fp);     // 跳过前面1行
    // fgets(buffer, 256, fp);
    // if (strstr(buffer, "OK") == NULL) {
    //     ret = -1;
    //     goto error;
    // }

    pclose(fp);
    fp = NULL;

    // 如果是当前网络，需要删除路由
    if((id < 0) || (id == net_id)) {
        snprintf(buffer, sizeof(buffer), "ip route flush dev %s", status.interface);
        fp = popen(buffer, "r");
        if (!fp) {
            perror("popen() failed");  
        }
    }

    ret = 0;

error:
    if (fp != NULL) {
        pclose(fp);
        fp = NULL;
    }
    return ret;
}

int main(int argc, char *argv[])
{
    int opt;

    static struct option longopts[] = {
        {"help", no_argument, NULL, 'h'},
        {"ping", no_argument, NULL, 'p'},
        {"get", no_argument, NULL, 'g'},
        {"dial", no_argument, NULL, 'b'},
        {"sharing", no_argument, NULL, 'f'},
        {"interface", required_argument, NULL, 'i'},
        {"scan", no_argument, NULL, 's'},
        {"list", no_argument, NULL, 'l'},
        {"status", no_argument, NULL, 't'},
        {"add", no_argument, NULL, 'a'},
        {"connect", no_argument, NULL, 'c'},
        {"dhcp", no_argument, NULL, 'd'},
        {"save", no_argument, NULL, 'e'},
        {"remove", no_argument, NULL, 'r'},
        {NULL, 0, NULL, 0}
    };
	while ((opt = getopt_long(argc, argv, "hpgb:f:i:slta:c:der:", longopts, NULL)) != -1) {
        switch (opt) {
        case 'h':
            usage(argv[0]);
            break;
        case 'p': {
            if (is_online()) {
                printf("系统已联网\n");
            } else {
                printf("系统未联网\n");
            }
        } break;
        case 'g': {
                char buff[128];
                int ret = get_network_scard(buff, 128);
                if(ret == 0) {
                    printf("当前系统使用的网卡是：%s\n", buff);
                }
            } break;
        case 'b': {
            if(strcmp(optarg, "up") == 0) {
                int status = system("pppd call quectel-ppp &");
                if (status != 0) {
                    printf("开始拨号失败！\n");
                } else {
                    printf("拨号成功！\n");
                }
            } else {
                int status = system("/etc/ppp/peers/quectel-ppp-kill");
                if (status != 0) {
                    printf("停止拨号失败！\n");
                } else {
                    printf("已停止拨号！\n");
                }
            }
        } break;
        case 'f': {
            char *flag = argv[2];
            char *ifname = argv[3];

            char buff[128];
            snprintf(buff, 128, "network_share.sh %s %s", flag, ifname);
            int status = system(buff);
            if (status != 0) {
                printf("%s 共享网络失败！\n", flag);
            } else {
                printf("%s 共享网络成功！\n", flag);
            }
        } break;
        case 'i': {
            if (select_wifi_interface(optarg) == 0) {
                printf("已选择网卡: %s\n", optarg);
            } else {
                printf("选择网卡失败: %s\n", optarg);
            }

        } break;
        case 's': {
            if (scan_wifi_networks() == 0) {
                printf("开始扫描热点\n");
            } else {
                printf("开始扫描失败\n");
            }
        } break;
        case 'l': {
            struct wifi_node networks[20];
            int networks_count = 0;
            int ret = get_wifi_networks(networks, 20, &networks_count);
            if(ret == 0) {
                printf("扫描到 %d 个wifi网络:\n", networks_count);
                for(int i = 0; i < networks_count; i++) {
                    printf("SSID: %s, BSSID: %s, Signal Level: %d, Frequency: %d\n",
                           networks[i].ssid, networks[i].bssid, networks[i].signal_level, networks[i].frequency);
                }
            } else {
                printf("获取wifi网络列表失败\n");
            }
        } break;
        case 't': {
            struct wifi_status status = {0};
            if (get_wifi_interface_status(&status) == 0) {
                printf("当前wifi状态:\n");
                printf("interface: %s\n", status.interface);
                printf("BSSID: %s\n", status.bssid);
                printf("Frequency: %d\n", status.frequency);
                printf("SSID: %s\n", status.ssid);
                printf("ID: %d\n", status.id);
                printf("Mode: %s\n", status.mode);
                printf("WPA State: %s\n", status.wpa_state);
                printf("IP Address: %s\n", status.ip_address);
                printf("MAC Address: %s\n", status.mac_address);
            } else {
                printf("获取wifi状态失败\n");
            }
        } break;
        case 'a': {
            char *ssid = argv[2];
            char *password = argv[3];
            printf("尝试连接到网络 SSID: %s, 密码: %s\n", ssid, password);
            if (add_wifi_networks(ssid, password) == 0) {
                printf("添加网络 %s 成功\n", ssid);
            } else {
                printf("添加网络 %s 失败\n", ssid);
            }
        } break;
        case 'c': {
            int id = atoi(optarg);
            if (connect_wifi_networks(id) == 0) {
                printf("连接网络 %d 成功\n", id);
            } else {
                printf("连接网络 %d 失败\n", id);
            }
        } break;
        case 'd': {
            if (get_wifi_ip() == 0) {
                printf("获取IP地址成功\n");
            } else {
                printf("获取IP地址失败\n");
            }
        } break;
        case 'e': {
            struct saved_wifi_node networks[10];
            int networks_count = 0;
            int ret = get_saved_wifi_networks(networks, 10, &networks_count);
            if(ret == 0) {
                printf("保存了 %d 个wifi网络:\n", networks_count);
                for(int i = 0; i < networks_count; i++) {
                    printf("ID: %d, SSID: %s, curr: %d\n",
                           networks[i].id, networks[i].ssid, networks[i].curr);
                }
            } else {
                printf("获取已保存的wifi网络列表失败\n");
            }
        } break;
        case 'r': {
            int id = atoi(optarg);
            if (remove_wifi_networks(id) == 0) {
                printf("删除网络成功\n");
            } else {
                printf("删除网络失败\n");
            }
        } break;
        default:
            printf("Error: invalid options. \n");
            usage(argv[0]);
            return -1;
        }
    }
}
