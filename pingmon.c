/*
 * pingmon - Advanced Ping Monitor
 * Copyright (c) 2026 zeroc
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction for non-commercial purposes, subject to
 * the following conditions:
 * 
 * 1. The above copyright notice and this permission notice shall be included in
 *    all copies or substantial portions of the Software.
 * 
 * 2. For commercial use, modification, or distribution, explicit written
 *    permission from the copyright holder is required.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 * 
 * For commercial licensing inquiries, contact: flyingzeroc@gmail.com
 */

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <regex.h>
#include <sys/wait.h>
#include <stdio.h>
#include <signal.h>
#include <math.h>
#include <time.h>
#include <termios.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <errno.h>

#define BUF_SIZE 512
#define HIST_SIZE 40

// ANSI Escape Codes
#define ANSI_RESET      "\033[0m"
#define ANSI_BOLD       "\033[1m"
#define ANSI_CLEAR_LINE "\033[K"
#define ANSI_HOME       "\033[H"
#define ANSI_CLEAR      "\033[2J"
#define ANSI_CURSOR_HIDE "\033[?25l"
#define ANSI_CURSOR_SHOW "\033[?25h"

// ANSI Colors
#define ANSI_BLACK   "\033[30m"
#define ANSI_RED     "\033[31m"
#define ANSI_GREEN   "\033[32m"
#define ANSI_YELLOW  "\033[33m"
#define ANSI_BLUE    "\033[34m"
#define ANSI_MAGENTA "\033[35m"
#define ANSI_CYAN    "\033[36m"
#define ANSI_WHITE   "\033[37m"

#define ANSI_ORANGE  "\033[38;5;214m"
#define ANSI_BG_BLACK "\033[40m"

// Cursor positionieren
#define CURSOR_POS(y, x) printf("\033[%d;%dH", (y), (x))

// MyIP-Struktur
typedef struct {
    char ip[64];
    char isp[128];
    char location[128];
    int fetched;
} IPInfo;

IPInfo my_ip = {0};

// Variablen umbenannt wegen Namenskonflikt mit socket.h
int packets_sent = 0;
int packets_recv = 0;
double sum = 0;
double last = 0;
time_t last_ping_time = 0;
int show_ip_info = 0;

// History-Puffer
double history[HIST_SIZE] = {0};
int hist_idx = 0;

// Global für Signal-Handler
pid_t ping_pid = -1;
struct termios saved_termios;

// Regex für ping-Ausgaben
regex_t re_time;

// ========== SICHERHEITSVERBESSERUNGEN ==========

// Signal-Handler für alle kritischen Signale
void cleanup_and_exit(int sig) {
    // Terminal zurücksetzen
    printf("%s%s", ANSI_CURSOR_SHOW, ANSI_RESET);
    tcsetattr(STDIN_FILENO, TCSANOW, &saved_termios);
    
    // Ping-Prozess sicher beenden
    if (ping_pid > 0) {
        kill(ping_pid, SIGTERM);
        int status;
        waitpid(ping_pid, &status, 0);  // Auf Beendigung warten
    }
    
    // Regex freigeben
    regfree(&re_time);
    
    fflush(stdout);
    exit(sig);
}

void reset_stats(void) {
    packets_sent = packets_recv = 0;
    sum = last = 0;
    last_ping_time = time(NULL);
    
    // History auch zurücksetzen
    memset(history, 0, sizeof(history));
    hist_idx = 0;
}

// IPv4-Validierungsfunktion
int is_valid_ipv4(const char *str) {
    struct sockaddr_in sa;
    return inet_pton(AF_INET, str, &(sa.sin_addr)) == 1;
}

// Wert zur History hinzufügen
void add_to_history(double value) {
    if (value > 0) {
        history[hist_idx] = value;
        hist_idx = (hist_idx + 1) % HIST_SIZE;
    }
}

// NEU: Farbe basierend auf WARN/CRIT (wie Quality-Balken)
const char* get_history_color(double value, double warn, double crit) {
    if (value >= crit) return ANSI_RED;
    if (value >= warn) return ANSI_MAGENTA;
    return ANSI_GREEN;
}

// History anzeigen (automatisch an Fußleiste ausgerichtet)
void draw_history(int line, int total_width, double warn, double crit) {
    CURSOR_POS(line, 1);
    
    // Korrekte Breitenberechnung:
    // "History: " = 9 Zeichen
    // " XXms" = 5 Zeichen (maximal 3 Ziffern + "ms")
    // Mindestabstand = 1 Leerzeichen
    int label_width = 15; // 9 + 5 + 1
    
    // Verfügbare Breite für die Grafik
    int graph_width = total_width - label_width;
    
    // Sicherstellen, dass wir sinnvolle Werte haben
    if (graph_width > HIST_SIZE) graph_width = HIST_SIZE;
    if (graph_width < 10) graph_width = 10;
    if (graph_width > total_width - 5) graph_width = total_width - 15;
    
    printf("%sHistory:%s ", ANSI_BOLD ANSI_WHITE, ANSI_RESET);
    
    // History-Grafik zeichnen MIT DEN SELBEN FARBEN WIE QUALITY
    for (int i = 0; i < graph_width; i++) {
        int idx = (hist_idx + i) % HIST_SIZE;
        double val = history[idx];
        
        if (val == 0) {
            printf("%s·%s", ANSI_WHITE, ANSI_RESET);
        } else {
            // WICHTIG: Gleiche Farblogik wie beim Quality-Balken!
            const char* color = get_history_color(val, warn, crit);
            printf("%s█%s", color, ANSI_RESET);
        }
    }
    
    // Letzten Wert als Zahl anzeigen (rechtsbündig)
    int last_idx = (hist_idx - 1 + HIST_SIZE) % HIST_SIZE;
    if (history[last_idx] > 0) {
        const char* color = get_history_color(history[last_idx], warn, crit);
        printf(" %s%.0fms%s", color, history[last_idx], ANSI_RESET);
    }
    
    // WICHTIG: Rest der Zeile löschen für exakte Breite
    // Wir müssen bis zum Ende der Fußleiste löschen
    int current_pos = label_width + graph_width + 6; // +6 für " XXms" + Farbcodes
    if (current_pos < total_width) {
        // Cursor ans Ende bewegen und Rest löschen
        CURSOR_POS(line, current_pos + 1);
        printf("%s", ANSI_CLEAR_LINE);
    }
}

// ========== SICHERES POPEN-REPLACEMENT ==========

// NEU: Sichere Ausführung von curl/wget ohne shell
int safe_exec_http_get(const char* url, char* buffer, size_t buf_size) {
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        return -1;
    }
    
    pid_t pid = fork();
    if (pid == -1) {
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }
    
    if (pid == 0) { // Child
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        
        // Sichere execvp ohne Shell
        if (strstr(url, "ipinfo.io") != NULL) {
            // Für ipinfo.io spezifische Kopfzeile
            char* argv[] = {"curl", "-s", "--max-time", "3", 
                           "-H", "Accept: application/json",
                           (char*)url, NULL};
            execvp("curl", argv);
        } else {
            // Allgemeiner Fall
            char* argv[] = {"curl", "-s", "--max-time", "3", (char*)url, NULL};
            execvp("curl", argv);
        }
        
        // Fallback zu wget wenn curl fehlschlägt
        char* wget_argv[] = {"wget", "-qO-", "--timeout=3", (char*)url, NULL};
        execvp("wget", wget_argv);
        
        _exit(127); // exec fehlgeschlagen
    }
    
    // Parent
    close(pipefd[1]);
    
    // Timeout für read
    fd_set fds;
    struct timeval tv = {3, 0}; // 3 Sekunden timeout
    
    FD_ZERO(&fds);
    FD_SET(pipefd[0], &fds);
    
    if (select(pipefd[0] + 1, &fds, NULL, NULL, &tv) > 0) {
        ssize_t bytes = read(pipefd[0], buffer, buf_size - 1);
        if (bytes > 0) {
            buffer[bytes] = '\0';
            // Newline entfernen
            buffer[strcspn(buffer, "\n\r")] = '\0';
        }
    }
    
    close(pipefd[0]);
    
    int status;
    waitpid(pid, &status, 0);
    
    return (WIFEXITED(status) && WEXITSTATUS(status) == 0) ? 0 : -1;
}

// NEU: MyIP-Abfrage mit sicheren execvp Aufrufen
void fetch_ip_info(IPInfo *info) {
    if (info->fetched) return;
    
    char buffer[128];
    
    // 1. IP-Adresse abrufen (mit mehreren Quellen für Redundanz)
    const char* ip_urls[] = {
        "http://ifconfig.me",
        "http://checkip.amazonaws.com",
        "http://ipinfo.io/ip",
        NULL
    };
    
    for (int i = 0; ip_urls[i] && strlen(info->ip) == 0; i++) {
        if (safe_exec_http_get(ip_urls[i], buffer, sizeof(buffer)) == 0) {
            if (strlen(buffer) >= 7) {
                int dots = 0;
                for (int j = 0; buffer[j]; j++) {
                    if (buffer[j] == '.') dots++;
                }
                if (dots >= 3 && is_valid_ipv4(buffer)) {
                    strncpy(info->ip, buffer, sizeof(info->ip) - 1);
                    info->ip[sizeof(info->ip) - 1] = '\0';
                }
            }
        }
    }
    
    // 2. ISP/Organisation abrufen (wenn IP gefunden)
    if (strlen(info->ip) > 0) {
        if (safe_exec_http_get("http://ipinfo.io/org", buffer, sizeof(buffer)) == 0) {
            if (strlen(buffer) > 0) {
                char *first_space = strchr(buffer, ' ');
                if (first_space) {
                    char *second_space = strchr(first_space + 1, ' ');
                    if (second_space) {
                        strncpy(info->isp, second_space + 1, sizeof(info->isp) - 1);
                        info->isp[sizeof(info->isp) - 1] = '\0';
                    } else {
                        strncpy(info->isp, first_space + 1, sizeof(info->isp) - 1);
                        info->isp[sizeof(info->isp) - 1] = '\0';
                    }
                } else {
                    strncpy(info->isp, buffer, sizeof(info->isp) - 1);
                    info->isp[sizeof(info->isp) - 1] = '\0';
                }
            } else {
                strcpy(info->isp, "Unknown ISP");
            }
        } else {
            strcpy(info->isp, "Unknown ISP");
        }
        
        // 3. Nur LANDESCODE abrufen
        if (safe_exec_http_get("http://ipinfo.io/country", buffer, sizeof(buffer)) == 0) {
            if (strlen(buffer) > 0 && strcmp(buffer, "undefined") != 0) {
                // Länderkürzel in Namen umwandeln
                if (strcmp(buffer, "DE") == 0) strcpy(info->location, "Germany");
                else if (strcmp(buffer, "US") == 0) strcpy(info->location, "USA");
                else if (strcmp(buffer, "GB") == 0) strcpy(info->location, "UK");
                else if (strcmp(buffer, "FR") == 0) strcpy(info->location, "France");
                else if (strcmp(buffer, "ES") == 0) strcpy(info->location, "Spain");
                else if (strcmp(buffer, "IT") == 0) strcpy(info->location, "Italy");
                else if (strcmp(buffer, "NL") == 0) strcpy(info->location, "Netherlands");
                else if (strcmp(buffer, "CH") == 0) strcpy(info->location, "Switzerland");
                else if (strcmp(buffer, "AT") == 0) strcpy(info->location, "Austria");
                else if (strcmp(buffer, "PL") == 0) strcpy(info->location, "Poland");
                else if (strcmp(buffer, "BE") == 0) strcpy(info->location, "Belgium");
                else if (strcmp(buffer, "SE") == 0) strcpy(info->location, "Sweden");
                else if (strcmp(buffer, "NO") == 0) strcpy(info->location, "Norway");
                else if (strcmp(buffer, "DK") == 0) strcpy(info->location, "Denmark");
                else if (strcmp(buffer, "FI") == 0) strcpy(info->location, "Finland");
                else if (strcmp(buffer, "CZ") == 0) strcpy(info->location, "Czech Republic");
                else if (strcmp(buffer, "HU") == 0) strcpy(info->location, "Hungary");
                else if (strcmp(buffer, "RO") == 0) strcpy(info->location, "Romania");
                else {
                    strncpy(info->location, buffer, sizeof(info->location) - 1);
                    info->location[sizeof(info->location) - 1] = '\0';
                }
            } else {
                strcpy(info->location, "Unknown");
            }
        } else {
            strcpy(info->location, "Unknown");
        }
    } else {
        strcpy(info->ip, "Not available");
        strcpy(info->isp, "No connection");
        strcpy(info->location, "Unknown");
    }
    
    info->fetched = 1;
}

// Hilfsfunktion: Farbe basierend auf Wert
const char* get_color(double value, double warn, double crit) {
    if (value >= crit) return ANSI_RED;
    if (value >= warn) return ANSI_MAGENTA;
    return ANSI_GREEN;
}

// Ping-Qualität berechnen
double calculate_quality(double last_ms) {
    if (last_ms <= 5) return 100.0;
    else if (last_ms <= 20) return 80.0 + (20.0 - last_ms) * 1.3333;
    else if (last_ms <= 30) return 60.0 + (30.0 - last_ms) * 2.0;
    else if (last_ms <= 60) return 30.0 + (60.0 - last_ms) * 1.0;
    else if (last_ms <= 100) return 10.0 + (100.0 - last_ms) * 0.5;
    else return 0.0;
}

// Stabilität berechnen
double calculate_stability(double loss_percent) {
    if (loss_percent <= 0.1) return 100.0;
    else if (loss_percent <= 1.0) return 100.0 - loss_percent * 10.0;
    else if (loss_percent <= 5.0) return 90.0 - (loss_percent - 1.0) * 15.0;
    else if (loss_percent <= 10.0) return 30.0 - (loss_percent - 5.0) * 6.0;
    else return 0.0;
}

// Dynamischen Balken zeichnen
void draw_dynamic_bar(double percentage, int length, const char* color) {
    int filled = (int)(percentage / 100.0 * length + 0.5);
    if (filled > length) filled = length;
    if (filled < 0) filled = 0;
    
    printf("%s", color);
    for (int i = 0; i < filled; i++) printf("█");
    for (int i = filled; i < length; i++) printf("░");
    printf("%s", ANSI_RESET);
}

// Rechtsbündige Ausrichtung
void draw_line_right(int line, const char* label, const char* value, const char* color, int width) {
    CURSOR_POS(line, 1);
    printf("%s%s%s", ANSI_BOLD ANSI_WHITE, label, ANSI_RESET);
    
    int value_len = strlen(value);
    int padding = (value_len < width) ? (width - value_len) : 0;
    
    CURSOR_POS(line, 20 - width);
    printf("%s%*s%s%s%s", color, padding, "", value, ANSI_CLEAR_LINE, ANSI_RESET);
}

// NEU: Sichere execvp für ping mit Fehlerbehandlung
int safe_start_ping(const char* target, int* pipefd) {
    if (pipe(pipefd) == -1) {
        return -1;
    }
    
    pid_t pid = fork();
    if (pid == -1) {
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }
    
    if (pid == 0) { // Child
        // Pipes schließen/umlenken
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        
        // Sichere execvp ohne Shell
        char* argv[] = {"ping", (char*)target, NULL};
        execvp("ping", argv);
        
        // Wenn execvp fehlschlägt
        _exit(127);
    }
    
    // Parent
    close(pipefd[1]);
    ping_pid = pid; // Für Signal-Handler speichern
    
    return 0;
}

int main(int argc, char *argv[]) {
    // Terminal-Einstellungen für Cleanup speichern
    tcgetattr(STDIN_FILENO, &saved_termios);
    
    // ========== ERWEITERTE SIGNAL-HANDLER ==========
    struct sigaction sa;
    sa.sa_handler = cleanup_and_exit;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    
    sigaction(SIGINT, &sa, NULL);   // Ctrl+C
    sigaction(SIGTERM, &sa, NULL);  // kill
    sigaction(SIGSEGV, &sa, NULL);  // Segmentation Fault
    sigaction(SIGPIPE, &sa, NULL);  // Broken Pipe
    sigaction(SIGABRT, &sa, NULL);  // Abort
    
    double warn = 30, crit = 60;
    char target[64] = "8.8.8.8";

    // Argument-Parsing mit Fehlerprüfung
    if (argc > 1) {
        char* endptr;
        warn = strtod(argv[1], &endptr);
        if (*endptr != '\0' || warn <= 0) {
            warn = 30; // Default bei Fehler
        }
    }
    
    if (argc > 2) {
        char* endptr;
        crit = strtod(argv[2], &endptr);
        if (*endptr != '\0' || crit <= warn) {
            crit = warn + 30; // Mindestens 30 über WARN
        }
    }
    
    if (argc > 3) {
        strncpy(target, argv[3], sizeof(target) - 1);
        target[sizeof(target) - 1] = '\0';
    }

    // IPv4-Validierung mit stiller Korrektur
    if (!is_valid_ipv4(target)) {
        strcpy(target, "8.8.8.8");
    }

    // Terminal auf raw mode setzen
    struct termios newt = saved_termios;
    newt.c_lflag &= ~(ICANON | ECHO);
    if (tcsetattr(STDIN_FILENO, TCSANOW, &newt) == -1) {
        fprintf(stderr, "Fehler: Terminal konnte nicht konfiguriert werden\n");
        return 1;
    }

    // Regex kompilieren mit Fehlerprüfung
    int regex_err = regcomp(&re_time, "time[=<]([0-9.]+)", REG_EXTENDED);
    if (regex_err != 0) {
        char regex_errbuf[100];
        regerror(regex_err, &re_time, regex_errbuf, sizeof(regex_errbuf));
        fprintf(stderr, "Regex Fehler: %s\n", regex_errbuf);
        tcsetattr(STDIN_FILENO, TCSANOW, &saved_termios);
        return 1;
    }
    
    // ========== SICHERES PING-STARTEN ==========
    int pipefd[2];
    if (safe_start_ping(target, pipefd) == -1) {
        fprintf(stderr, "Fehler: Ping konnte nicht gestartet werden\n");
        regfree(&re_time);
        tcsetattr(STDIN_FILENO, TCSANOW, &saved_termios);
        return 1;
    }
    
    // Non-blocking setzen
    fcntl(pipefd[0], F_SETFL, O_NONBLOCK);
    
    reset_stats();
    time_t last_success_time = time(NULL);
    int timeout_state = 0;
    int running = 1;

    char line[BUF_SIZE];
    int line_len = 0;

    // Terminal vorbereiten und Cursor unsichtbar machen
    printf("%s%s%s", ANSI_HOME, ANSI_CLEAR, ANSI_CURSOR_HIDE);
    fflush(stdout);

    // Fußzeilen-Text (OHNE Version, nur Beschreibung)
    char footer[] = "© zeroc 2026 | pingmon [warn] [crit] [target] | e.g., pingmon 50 100 1.1.1.1";
    int footer_len = strlen(footer);

    // Balkenlängen berechnen
    int labels_len = 21;
    int available_for_bars = footer_len - labels_len;
    int bar_length = (available_for_bars > 0) ? available_for_bars / 2 : 5;
    if (bar_length < 5) bar_length = 5;
    if (bar_length > 30) bar_length = 30;

    // Statische Kopfzeile zeichnen (MIT Version in der Kopfzeile)
    CURSOR_POS(1, 1);
    printf("%sPing Monitor v0.39%s%s", ANSI_BOLD ANSI_WHITE, ANSI_CLEAR_LINE, ANSI_RESET);
    
    CURSOR_POS(2, 1);
    printf("%sTarget: %s%s%s", ANSI_WHITE, target, ANSI_CLEAR_LINE, ANSI_RESET);
    
    CURSOR_POS(3, 1);
    printf("%sWARN %.0f ms | CRIT %.0f ms%s%s", ANSI_WHITE, warn, crit, ANSI_CLEAR_LINE, ANSI_RESET);
    
    CURSOR_POS(4, 1);
    printf("%sKeys: q=quit  r=reset  m=myIP%s%s", ANSI_WHITE, ANSI_CLEAR_LINE, ANSI_RESET);
    
    // Trennlinie
    CURSOR_POS(5, 1);
    printf("%s", ANSI_WHITE);
    for (int i = 0; i < footer_len; i++) printf("-");
    printf("%s%s", ANSI_CLEAR_LINE, ANSI_RESET);
    
    fflush(stdout);

    #define VALUE_WIDTH 8

    while (running) {
        char c;
        ssize_t bytes_read;
        
        // ========== SICHERES READ MIT FEHLERBEHANDLUNG ==========
        while ((bytes_read = read(pipefd[0], &c, 1)) > 0) {
            if (c == '\n') {
                line[line_len] = '\0';
                
                regmatch_t m[2];
                
                if (regexec(&re_time, line, 2, m, 0) == 0) {
                    packets_sent++;
                    packets_recv++;
                    
                    char tmp[16];
                    int len = m[1].rm_eo - m[1].rm_so;
                    if (len < (int)sizeof(tmp)) {
                        strncpy(tmp, line + m[1].rm_so, len);
                        tmp[len] = '\0';

                        last = atof(tmp);
                        sum += last;
                        last_ping_time = time(NULL);
                        last_success_time = last_ping_time;
                        timeout_state = 0;
                        
                        add_to_history(last);
                    }
                }
                
                line_len = 0;
            } else if (line_len < BUF_SIZE - 2) {
                line[line_len++] = c;
            } else {
                // Puffer voll, zurücksetzen
                line_len = 0;
            }
        }
        
        // Fehlerbehandlung für read
        if (bytes_read == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
            // Ping-Prozess ist wahrscheinlich beendet
            timeout_state = 1;
        }

        // Timeout-Erkennung
        if (last_success_time > 0) {
            time_t now = time(NULL);
            if (now - last_success_time > 2) {
                if (packets_sent == packets_recv) {
                    packets_sent++;
                }
                timeout_state = 1;
            } else {
                timeout_state = 0;
            }
        }

        // Tastatureingabe
        struct timeval tv = {0, 0};
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        
        if (select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0) {
            char ch = getchar();
            if (ch == 'q') running = 0;
            if (ch == 'r') {
                reset_stats();
                last_success_time = time(NULL);
                timeout_state = 0;
            }
            if (ch == 'm') {
                if (!my_ip.fetched) {
                    fetch_ip_info(&my_ip);
                }
                show_ip_info = !show_ip_info;
            }
        }

        // Zeile 6: MyIP-Info
        CURSOR_POS(6, 1);
        if (show_ip_info && my_ip.fetched) {
            printf("%sMyIP: ", ANSI_BOLD ANSI_WHITE);
            printf("%s%s", ANSI_MAGENTA, my_ip.ip);
            printf("%s | ISP: ", ANSI_BOLD ANSI_WHITE);
            printf("%s%s", ANSI_MAGENTA, my_ip.isp);
            printf("%s | Location: ", ANSI_BOLD ANSI_WHITE);
            printf("%s%s%s%s", ANSI_MAGENTA, my_ip.location, ANSI_CLEAR_LINE, ANSI_RESET);
        } else {
            printf("%s", ANSI_CLEAR_LINE);
        }
        
        // Zeile 7: Quality & Stability Balken
        CURSOR_POS(7, 1);
        
        double quality = calculate_quality(last);
        double loss = (packets_sent > 0) ? (packets_sent - packets_recv) * 100.0 / packets_sent : 0.0;
        double stability = calculate_stability(loss);
        
        printf("%sQuality: ", ANSI_BOLD ANSI_WHITE);
        
        // Quality-Balken
        if (quality >= 80) printf("%s", ANSI_GREEN);
        else if (quality >= 60) printf("%s", ANSI_YELLOW);
        else if (quality >= 40) printf("%s", ANSI_ORANGE);
        else printf("%s", ANSI_RED);
        
        draw_dynamic_bar(quality, bar_length, "");
        printf("%s | %sStability: ", ANSI_RESET, ANSI_BOLD ANSI_WHITE);
        
        // Stability-Balken
        if (stability >= 90) printf("%s", ANSI_GREEN);
        else if (stability >= 70) printf("%s", ANSI_YELLOW);
        else if (stability >= 50) printf("%s", ANSI_ORANGE);
        else printf("%s", ANSI_RED);
        
        draw_dynamic_bar(stability, bar_length, "");
        printf(" %.0f%%%s%s", stability, ANSI_CLEAR_LINE, ANSI_RESET);
        
        // Zeile 8: History
        draw_history(8, footer_len, warn, crit);
        
        // Zeile 9: Leerzeile
        CURSOR_POS(9, 1);
        printf("%s", ANSI_CLEAR_LINE);
        
        // Metriken
        char last_buf[32];
        snprintf(last_buf, sizeof(last_buf), "%.1f ms", last);
        draw_line_right(10, "Last:", last_buf, get_color(last, warn, crit), VALUE_WIDTH);
        
        double avg = packets_recv > 0 ? sum / packets_recv : 0.0;
        char avg_buf[32];
        snprintf(avg_buf, sizeof(avg_buf), "%.1f ms", avg);
        draw_line_right(11, "Avg :", avg_buf, get_color(avg, warn, crit), VALUE_WIDTH);
        
        char loss_buf[32];
        snprintf(loss_buf, sizeof(loss_buf), "%.2f %%", loss);
        const char* loss_color = (loss > 0) ? ANSI_YELLOW : ANSI_GREEN;
        draw_line_right(12, "Loss:", loss_buf, loss_color, VALUE_WIDTH);
        
        char status_buf[32];
        const char* status_color;
        if (timeout_state) {
            snprintf(status_buf, sizeof(status_buf), "TIMEOUT");
            status_color = ANSI_RED;
        } else {
            snprintf(status_buf, sizeof(status_buf), "OK");
            status_color = ANSI_GREEN;
        }
        draw_line_right(13, "Status:", status_buf, status_color, VALUE_WIDTH);
        
        char sr_buf[32];
        snprintf(sr_buf, sizeof(sr_buf), "%d/%d", packets_sent, packets_recv);
        draw_line_right(14, "Sent/Recv:", sr_buf, ANSI_WHITE, VALUE_WIDTH);
        
        // Copyright-Fußzeile (OHNE Version)
        CURSOR_POS(16, 1);
        printf("%s%s%s%s", ANSI_WHITE, footer, ANSI_CLEAR_LINE, ANSI_RESET);
        
        fflush(stdout);
        
        usleep(100000);
    }

    // ========== SAUBERES BEENDEN ==========
    printf("%s%s%s", ANSI_CURSOR_SHOW, ANSI_CLEAR, ANSI_HOME);
    
    // Ping-Prozess beenden
    if (ping_pid > 0) {
        kill(ping_pid, SIGTERM);
        int status;
        waitpid(ping_pid, &status, 0);  // Warten auf Beendigung
    }
    
    close(pipefd[0]);
    regfree(&re_time);
    tcsetattr(STDIN_FILENO, TCSANOW, &saved_termios);
    
    return 0;
}
