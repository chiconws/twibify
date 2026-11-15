/* iotweb - backend do chiconws (porta 8080)
 *
 * API usada pela pagina /iot.html dentro da webui original:
 *   GET    /api/status         versao, chave ssh, bloqueio iot
 *   GET    /api/clients        clientes vistos (arp) + flag iot
 *   POST   /api/iot            {"mac":"..","iot":true|false}
 *   GET    /api/ssh            {"key":bool}
 *   POST   /api/ssh/password   {"password":".."}
 *   POST   /api/ssh/key        {"key":"ssh-ed25519 ..."}
 *   DELETE /api/ssh/key
 *
 * Autenticacao: Bearer token lido de /webroot/iot.token (o GoAhead so
 * entrega esse arquivo para sessao logada). CORS liberado porque o
 * servico so escuta na LAN.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "md5crypt.c"

#define PORT 8080
#define VERSION_FILE "/etc/chiconws_version"
#define TOKEN_FILE "/webroot/iot.token"
#define PASSWD_FILE "/etc/passwd"
#define AUTH_KEYS "/var/root/.ssh/authorized_keys"
#define IOT_CLIENTS_DEFAULT "/usr/sbin/iot-clients"
static const char *iot_clients = IOT_CLIENTS_DEFAULT;

static char token[128];

static void rstrip(char *s) {
    size_t n = strlen(s);
    while (n && (s[n - 1] == '\n' || s[n - 1] == '\r' || s[n - 1] == ' ')) s[--n] = 0;
}

static int read_file(const char *path, char *buf, size_t cap) {
    FILE *f = fopen(path, "r");
    size_t n;
    if (!f) return -1;
    n = fread(buf, 1, cap - 1, f);
    fclose(f);
    buf[n] = 0;
    return (int)n;
}

static int write_file(const char *path, const char *data, mode_t mode) {
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fputs(data, f);
    fclose(f);
    chmod(path, mode);
    return 0;
}

static void load_token(void) {
    token[0] = 0;
    if (read_file(TOKEN_FILE, token, sizeof(token)) < 0) token[0] = 0;
    rstrip(token);
}

static void json_escape(const char *in, char *out, size_t cap) {
    size_t o = 0;
    for (; *in && o + 8 < cap; in++) {
        unsigned char c = (unsigned char)*in;
        if (c == '"' || c == '\\') { out[o++] = '\\'; out[o++] = c; }
        else if (c == '\n') { out[o++] = '\\'; out[o++] = 'n'; }
        else if (c < 0x20) { o += snprintf(out + o, cap - o, "\\u%04x", c); }
        else out[o++] = c;
    }
    out[o] = 0;
}

/* extrai "campo":"valor" (string) do corpo json */
static int json_string(const char *body, const char *key, char *out, size_t cap) {
    char pat[64];
    const char *p;
    size_t o = 0;
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    p = strstr(body, pat);
    if (!p) return -1;
    p = strchr(p + strlen(pat), ':');
    if (!p) return -1;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '"') return -1;
    p++;
    while (*p && *p != '"' && o + 1 < cap) {
        if (*p == '\\' && p[1]) p++;
        out[o++] = *p++;
    }
    out[o] = 0;
    return 0;
}

static int json_bool(const char *body, const char *key) {
    char pat[64];
    const char *p;
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    p = strstr(body, pat);
    if (!p) return 0;
    p = strchr(p, ':');
    if (!p) return 0;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    return (*p == 't' || *p == '1');
}

static int valid_mac(const char *m) {
    int i, n = 0;
    for (i = 0; m[i]; i++) {
        if (m[i] == ':') { n++; continue; }
        if (!isxdigit((unsigned char)m[i])) return 0;
    }
    return n == 5 && strlen(m) == 17;
}

static int valid_password(const char *p) {
    size_t i, n = strlen(p);
    if (n < 8 || n > 64) return 0;
    for (i = 0; i < n; i++) {
        if (!isprint((unsigned char)p[i]) || p[i] == ':' || p[i] == ' ' || p[i] == '\'') return 0;
    }
    return 1;
}

static int valid_key(const char *k) {
    size_t i, n = strlen(k);
    if (n < 40 || n > 2000) return 0;
    if (strncmp(k, "ssh-ed25519 ", 12) && strncmp(k, "ssh-rsa ", 8) &&
        strncmp(k, "ecdsa-sha2-nistp256 ", 20) && strncmp(k, "ssh-dss ", 8)) return 0;
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)k[i];
        if (c < 0x20 || c > 0x7e) return 0;
    }
    return 1;
}

/* troca o hash da senha do root em /etc/passwd (o dropbear le daqui) */
static int set_root_hash(const char *hash) {
    char buf[8192], out[8192];
    char *line, *next, *o = out;
    int found = 0;
    if (read_file(PASSWD_FILE, buf, sizeof(buf)) < 0) return -1;
    line = buf;
    while (line && *line) {
        char *nl = strchr(line, '\n');
        size_t len;
        if (nl) { *nl = 0; next = nl + 1; } else next = NULL;
        len = strlen(line);
        if (!found && !strncmp(line, "root:", 5)) {
            char *c2 = strchr(line + 5, ':');
            o += sprintf(o, "root:%s%s", hash, c2 ? c2 : "::0:0:root:/:/bin/sh");
            found = 1;
        } else {
            memcpy(o, line, len);
            o += len;
        }
        *o++ = '\n';
        line = next;
    }
    *o = 0;
    if (!found) return -1;
    if (write_file(PASSWD_FILE, out, 0644) < 0) return -1;
    return 0;
}

static void random_salt(char *s, int n) {
    static const char itoa64[] =
        "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    unsigned char rnd[16];
    int i, fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) { (void)!read(fd, rnd, sizeof(rnd)); close(fd); }
    else { for (i = 0; i < 16; i++) rnd[i] = (unsigned char)(rand() & 0xff); }
    for (i = 0; i < n; i++) s[i] = itoa64[rnd[i] % 64];
    s[n] = 0;
}

static void cfm_set(const char *key, const char *val) {
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "cfm Set %s '%s' >/dev/null 2>&1", key, val);
    (void)!system(cmd);
}

/* ----------------------------- respostas ----------------------------- */

static void send_cors(int fd) {
    dprintf(fd, "Access-Control-Allow-Origin: *\r\n");
    dprintf(fd, "Access-Control-Allow-Headers: Authorization, Content-Type\r\n");
    dprintf(fd, "Access-Control-Allow-Methods: GET, POST, DELETE, OPTIONS\r\n");
}

static void send_json(int fd, int code, const char *body) {
    dprintf(fd, "HTTP/1.1 %d %s\r\n", code, code == 200 ? "OK" : "Error");
    dprintf(fd, "Content-Type: application/json\r\n");
    dprintf(fd, "Content-Length: %zu\r\n", strlen(body));
    send_cors(fd);
    dprintf(fd, "Connection: close\r\n\r\n%s", body);
}

static void send_empty(int fd, int code) {
    dprintf(fd, "HTTP/1.1 %d X\r\n", code);
    send_cors(fd);
    dprintf(fd, "Content-Length: 0\r\nConnection: close\r\n\r\n");
}

static int authorized(const char *req) {
    const char *p = strstr(req, "Authorization: Bearer ");
    if (!p) p = strstr(req, "authorization: bearer ");
    if (!p || !token[0]) return 0;
    p += 22;
    return !strncmp(p, token, strlen(token));
}

/* ------------------------------- rotas ------------------------------- */

static void run_capture(const char *cmd, char *out, int outlen) {
    FILE *p = popen(cmd, "r");
    int n = 0;
    out[0] = 0;
    if (!p) return;
    n = (int)fread(out, 1, outlen - 1, p);
    out[n] = 0;
    pclose(p);
}

static void api_status(int fd) {
    char ver[128] = "desconhecida", mi[1024], th[4096], up[64], body[1024];
    long total = 0, freem = 0, buf = 0, cache = 0, avail = 0, t24 = 0, t5 = 0;
    int has_key = 0, iot = 0;
    char *t, *f, *b, *c, *a, *l1;

    read_file(VERSION_FILE, ver, sizeof(ver));
    rstrip(ver);
    {
        char kbuf[256];
        if (read_file(AUTH_KEYS, kbuf, sizeof(kbuf)) > 0 && strstr(kbuf, "ssh-")) has_key = 1;
        if (read_file("/tmp/iot.count", kbuf, sizeof(kbuf)) > 0) iot = atoi(kbuf);
    }
    read_file("/proc/meminfo", mi, sizeof(mi));
    t = strstr(mi, "MemTotal:"); f = strstr(mi, "MemFree:");
    b = strstr(mi, "Buffers:"); c = strstr(mi, "Cached:"); a = strstr(mi, "MemAvailable:");
    if (t) total = atol(t + 9);
    if (f) freem = atol(f + 8);
    if (b) buf = atol(b + 8);
    if (c) cache = atol(c + 7);
    if (a) avail = atol(a + 13); else avail = freem + buf + cache;
    if (read_file("/proc/wlan0/mib_all", th, sizeof(th)) > 0) {
        l1 = strstr(th, "current thermal:");
        if (l1) t24 = atol(l1 + 16);
    }
    if (read_file("/proc/wlan1/mib_all", th, sizeof(th)) > 0) {
        l1 = strstr(th, "current thermal:");
        if (l1) t5 = atol(l1 + 16);
    }
    read_file("/proc/uptime", up, sizeof(up));
    snprintf(body, sizeof(body),
             "{\"version\":\"%s\",\"ssh_key\":%s,\"iot_clients\":%d,"
             "\"mem_total_kb\":%ld,\"mem_used_kb\":%ld,\"mem_used_pct\":%ld,"
             "\"thermal_2g\":%ld,\"thermal_5g\":%ld,\"uptime_s\":%ld}",
             ver, has_key ? "true" : "false", iot,
             total, total - avail, total ? (100 * (total - avail)) / total : 0,
             t24, t5, (long)atol(up));
    send_json(fd, 200, body);
}

static void api_clients(int fd) {
    char arp[8192], out[16384], list[2048];
    char *line, *o = out, *saveptr = NULL;
    o += sprintf(o, "[");
    list[0] = 0;
    {
        char cmd[256];
        FILE *p;
        snprintf(cmd, sizeof(cmd), "%s list 2>/dev/null", iot_clients);
        p = popen(cmd, "r");
        if (p) {
            size_t n = fread(list, 1, sizeof(list) - 1, p);
            list[n] = 0;
            pclose(p);
        }
    }
    if (read_file("/proc/net/arp", arp, sizeof(arp)) > 0) {
        line = strtok_r(arp, "\n", &saveptr);
        if (line) line = strtok_r(NULL, "\n", &saveptr);
        while (line) {
            char ip[64] = "", mac[64] = "";
            if (sscanf(line, "%63s %*s %*s %63s", ip, mac) == 2 &&
                strcmp(mac, "00:00:00:00:00:00") != 0) {
                char lmac[64];
                int i, is_iot = 0;
                for (i = 0; mac[i]; i++) lmac[i] = tolower((unsigned char)mac[i]);
                lmac[i] = 0;
                if (strstr(list, lmac)) is_iot = 1;
                if (o != out + 1) o += sprintf(o, ",");
                o += sprintf(o, "{\"ip\":\"%s\",\"mac\":\"%s\",\"iot\":%s}",
                             ip, lmac, is_iot ? "true" : "false");
                if (o > out + sizeof(out) - 256) break;
            }
            line = strtok_r(NULL, "\n", &saveptr);
        }
    }
    sprintf(o, "]");
    send_json(fd, 200, out);
}

static void api_iot(int fd, const char *body) {
    char mac[64], cmd[256];
    int on;
    if (json_string(body, "mac", mac, sizeof(mac)) < 0 || !valid_mac(mac)) {
        send_json(fd, 400, "{\"error\":\"mac invalido\"}");
        return;
    }
    on = json_bool(body, "iot");
    snprintf(cmd, sizeof(cmd), "%s %s %s >/dev/null 2>&1", iot_clients, on ? "add" : "del", mac);
    (void)!system(cmd);
    send_json(fd, 200, "{\"ok\":true}");
}

static int ssh_enabled(void) {
    char b[64] = "";
    FILE *p = popen("cfm Get ssh.enabled 2>/dev/null", "r");
    if (p) {
        size_t n = fread(b, 1, sizeof(b) - 1, p);
        b[n] = 0;
        pclose(p);
    }
    rstrip(b);
    if (strstr(b, "failed")) return 0;
    return atoi(b) == 1;
}

static void api_ssh_get(int fd) {
    char buf[256] = "";
    int has_key = 0, en = ssh_enabled();
    if (read_file(AUTH_KEYS, buf, sizeof(buf)) > 0 && strstr(buf, "ssh-")) has_key = 1;
    send_json(fd, 200, has_key
        ? (en ? "{\"enabled\":true,\"key\":true}" : "{\"enabled\":false,\"key\":true}")
        : (en ? "{\"enabled\":true,\"key\":false}" : "{\"enabled\":false,\"key\":false}"));
}

static void api_ssh_toggle(int fd, const char *body, int on) {
    if (!on) {
        cfm_set("ssh.enabled", "0");
        (void)!system("killall dropbear 2>/dev/null");
        send_json(fd, 200, "{\"ok\":true}");
        return;
    }
    {
        char pw[128], salt[9], hash[64];
        if (json_string(body, "password", pw, sizeof(pw)) < 0 || !valid_password(pw)) {
            send_json(fd, 400, "{\"error\":\"defina uma senha (minimo 8, sem espaco) para ligar o SSH\"}");
            return;
        }
        random_salt(salt, 8);
        md5_crypt(pw, salt, hash);
        {
            char sh[8192], sout[8192], *sl, *sn, *so = sout;
            if (read_file("/etc/shadow", sh, sizeof(sh)) >= 0) {
                int f = 0;
                sl = sh;
                while (sl && *sl) {
                    sn = strchr(sl, '\n');
                    if (sn) { *sn = 0; sn++; }
                    if (!f && !strncmp(sl, "root:", 5)) {
                        char *c2 = strchr(sl + 5, ':');
                        so += sprintf(so, "root:%s%s", hash, c2 ? c2 : ":14319::::::");
                        f = 1;
                    } else {
                        so += sprintf(so, "%s", sl);
                    }
                    *so++ = '\n';
                    sl = sn;
                }
                *so = 0;
                if (f) write_file("/etc/shadow", sout, 0644);
            }
        }
        if (set_root_hash(hash) < 0) {
            send_json(fd, 500, "{\"error\":\"falha ao gravar /etc/passwd\"}");
            return;
        }
        cfm_set("ssh.pwd", hash);
        cfm_set("ssh.enabled", "1");
        (void)!system("(dropbear -p 22 -b /etc_ro/issue -P /var/run/dropbear.pid >/dev/null 2>&1 &)");
        send_json(fd, 200, "{\"ok\":true}");
    }
}

static void api_ssh_password(int fd, const char *body) {
    char pw[128], salt[9], hash[64];
    if (json_string(body, "password", pw, sizeof(pw)) < 0 || !valid_password(pw)) {
        send_json(fd, 400, "{\"error\":\"senha fraca: minimo 8 caracteres, sem espaco\"}");
        return;
    }
    random_salt(salt, 8);
    md5_crypt(pw, salt, hash);
    {
        char sh[8192], sout[8192], *sl, *sn, *so = sout;
        if (read_file("/etc/shadow", sh, sizeof(sh)) >= 0) {
            int f = 0;
            sl = sh;
            while (sl && *sl) {
                sn = strchr(sl, '\n');
                if (sn) { *sn = 0; sn++; }
                if (!f && !strncmp(sl, "root:", 5)) {
                    char *c2 = strchr(sl + 5, ':');
                    so += sprintf(so, "root:%s%s", hash, c2 ? c2 : ":14319::::::");
                    f = 1;
                } else {
                    so += sprintf(so, "%s", sl);
                }
                *so++ = '\n';
                sl = sn;
            }
            *so = 0;
            if (f) write_file("/etc/shadow", sout, 0644);
        }
    }
    if (set_root_hash(hash) < 0) {
        send_json(fd, 500, "{\"error\":\"falha ao gravar /etc/passwd\"}");
        return;
    }
    cfm_set("ssh.pwd", hash);
    send_json(fd, 200, "{\"ok\":true}");
}

static void api_ssh_key(int fd, const char *body, int del) {
    char key[2048];
    if (del) {
        unlink(AUTH_KEYS);
        cfm_set("ssh.key", "");
        send_json(fd, 200, "{\"ok\":true}");
        return;
    }
    if (json_string(body, "key", key, sizeof(key)) < 0 || !valid_key(key)) {
        send_json(fd, 400, "{\"error\":\"chave invalida\"}");
        return;
    }
    mkdir("/var/root", 0700);
    mkdir("/var/root/.ssh", 0700);
    chmod("/var/root/.ssh", 0700);
    if (write_file(AUTH_KEYS, key, 0600) < 0) {
        send_json(fd, 500, "{\"error\":\"falha ao gravar authorized_keys\"}");
        return;
    }
    cfm_set("ssh.key", key);
    send_json(fd, 200, "{\"ok\":true}");
}

/* ------------------------------- main ------------------------------- */

static int handle(int fd, const char *req, const char *body, const char *method, const char *path) {
    if (!strcmp(method, "OPTIONS")) { send_empty(fd, 204); return 0; }
    if (!authorized(req)) { send_json(fd, 401, "{\"error\":\"nao autorizado\"}"); return 0; }
    if (!strcmp(path, "/api/status")) return api_status(fd), 0;
    if (!strcmp(path, "/api/clients")) return api_clients(fd), 0;
    if (!strcmp(path, "/api/ssh")) return api_ssh_get(fd), 0;
    if (!strcmp(path, "/api/iot") && !strcmp(method, "POST")) return api_iot(fd, body), 0;
    if (!strcmp(path, "/api/ssh/password") && !strcmp(method, "POST")) return api_ssh_password(fd, body), 0;
    if (!strcmp(path, "/api/ssh/enable") && !strcmp(method, "POST")) return api_ssh_toggle(fd, body, 1), 0;
    if (!strcmp(path, "/api/ssh/disable") && !strcmp(method, "POST")) return api_ssh_toggle(fd, body, 0), 0;
    if (!strcmp(path, "/api/ssh/key") && !strcmp(method, "POST")) return api_ssh_key(fd, body, 0), 0;
    if (!strcmp(path, "/api/ssh/key") && !strcmp(method, "DELETE")) return api_ssh_key(fd, body, 1), 0;
    send_json(fd, 404, "{\"error\":\"rota desconhecida\"}");
    return 0;
}

int main(void) {
    int srv, cli;
    struct sockaddr_in addr;
    if (chdir("/")) {}
    srand((unsigned)time(NULL));
    if (getenv("IOT_CLIENTS")) iot_clients = getenv("IOT_CLIENTS");

    srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) return 1;
    {
        int one = 1;
        setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    }
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PORT);
    if (bind(srv, (struct sockaddr *)&addr, sizeof(addr)) < 0) return 1;
    listen(srv, 8);

    for (;;) {
        char req[8192], body[8192];
        char method[16] = "", path[256] = "";
        size_t n = 0, clen = 0;
        int hdr_done = 0;
        char *p;
        cli = accept(srv, NULL, NULL);
        if (cli < 0) continue;
        memset(req, 0, sizeof(req));
        while (n < sizeof(req) - 1) {
            ssize_t r = recv(cli, req + n, sizeof(req) - 1 - n, 0);
            if (r <= 0) break;
            n += r;
            req[n] = 0;
            if (!hdr_done && (p = strstr(req, "\r\n\r\n")) != NULL) {
                hdr_done = 1;
                p += 4;
                if (strstr(req, "Content-Length:") || strstr(req, "content-length:")) {
                    p = strstr(req, "Content-Length:");
                    if (!p) p = strstr(req, "content-length:");
                    clen = (size_t)atoi(p + 15);
                }
                if (strlen(p) >= clen) break;
            }
        }
        body[0] = 0;
        if ((p = strstr(req, "\r\n\r\n")) != NULL) {
            size_t have = n - (size_t)(p + 4 - req);
            size_t take = have < clen ? have : clen;
            if (take >= sizeof(body)) take = sizeof(body) - 1;
            memcpy(body, p + 4, take);
            body[take] = 0;
        }
        if (sscanf(req, "%15s %255s", method, path) == 2) {
            char *q = strchr(path, '?');
            if (q) *q = 0;
            load_token();
            handle(cli, req, body, method, path);
        }
        close(cli);
    }
    return 0;
}
