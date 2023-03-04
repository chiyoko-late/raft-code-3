#include "appendentries.h"
#include "debug.h"
// #define NULL __DARWIN_NULL

int AppendEntriesRPC(
    int connectserver_num,
    int sock[],
    struct AppendEntriesRPC_Argument *AERPC_A,
    struct AppendEntriesRPC_Result *AERPC_R,
    struct Leader_VolatileState *L_VS,
    struct AllServer_VolatileState *AS_VS,
    struct AllServer_PersistentState *AS_PS)
{
    int replicatelog_num = 0;

    /* AERPC_Aの設定 */

    AERPC_A->term = AS_PS->log[L_VS->nextIndex[0]].term;
    // AERPC_A->term = 1;
    AERPC_A->prevLogIndex = L_VS->nextIndex[0] - 1;
    AERPC_A->prevLogTerm = AS_PS->log[AERPC_A->prevLogIndex].term;
    // AERPC_A->prevLogTerm = 0;
    for (int i = 1; i < ONCE_SEND_ENTRIES; i++)
    {
        strcpy(AERPC_A->entries[i - 1].entry, AS_PS->log[L_VS->nextIndex[0] + (i - 1)].entry);
    }
    AERPC_A->leaderCommit = AS_VS->commitIndex;

    // output_AERPC_A(AERPC_A);

    for (int i = 0; i < connectserver_num; i++)
    {

        my_send(sock[i], AERPC_A, sizeof(struct AppendEntriesRPC_Argument));

        // for (int num = 1; num < ONCE_SEND_ENTRIES; num++)
        // {
        //     // send(sock[i], AERPC_A->entries[num - 1], sizeof(char) * MAX, 0);
        //     my_send(sock[i], AERPC_A->entries[num - 1].entry, sizeof(char) * STRING_MAX);
        // }
    }
    printf("finish sending\n\n");
    for (int i = 0; i < connectserver_num; i++)
    {
        // recv(sock[i], AERPC_R, sizeof(struct AppendEntriesRPC_Result), MSG_WAITALL);
        my_recv(sock[i], AERPC_R, sizeof(struct AppendEntriesRPC_Result));
    }

    for (int i = 0; i < connectserver_num; i++)
    {
        // output_AERPC_R(AERPC_R);
        printf("send to server %d\n", i);

        // • If successful: update nextIndex and matchIndex for follower.
        if (AERPC_R->success == 1)
        {
            L_VS->nextIndex[i] += (ONCE_SEND_ENTRIES - 1);
            L_VS->matchIndex[i] += (ONCE_SEND_ENTRIES - 1);

            replicatelog_num += (ONCE_SEND_ENTRIES - 1);

            printf("Success:%d\n", i);
        }
        // • If AppendEntries fails because of log inconsistency: decrement nextIndex and retry.
        else
        {
            printf("failure0\n");
            L_VS->nextIndex[i] -= (ONCE_SEND_ENTRIES - 1);
            AERPC_A->prevLogIndex -= (ONCE_SEND_ENTRIES - 1);
            AppendEntriesRPC(connectserver_num, sock, AERPC_A, AERPC_R, L_VS, AS_VS, AS_PS);
            printf("failure1\n");
            exit(1);
        }
    }
    // AS_VS->commitIndex += 1;
    return replicatelog_num;
}

int main(int argc, char *argv[])
{
    int port[4];
    port[0] = 1234;
    port[1] = 2345;
    port[2] = 3456;
    port[3] = 4567;

    int sock[4];
    struct sockaddr_in addr[4];

    char *ip[4];
    ip[0] = argv[2];
    ip[1] = argv[3];
    ip[2] = argv[4];
    ip[3] = argv[5];

    /* ソケットを作成 */
    for (int i = 0; i < 4; i++)
    {
        sock[i] = socket(AF_INET, SOCK_STREAM, 0);
        if (sock[i] < 0)
        {
            perror("socket error ");
            exit(0);
        }
    }

    for (int i = 0; i < 4; i++)
    {
        memset(&addr[i], 0, sizeof(struct sockaddr_in));
    }
    /* サーバーのIPアドレスとポートの情報を設定 */
    for (int i = 0; i < 4; i++)
    {
        addr[i].sin_family = AF_INET;
        addr[i].sin_port = htons(port[i]);
        addr[i].sin_addr.s_addr = inet_addr(ip[i]);
        const size_t addr_size = sizeof(addr);
    }

    int opt = 1;
    // ポートが解放されない場合, SO_REUSEADDRを使う
    for (int i = 0; i < 4; i++)
    {
        if (setsockopt(sock[i], SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt)) == -1)
        {
            perror("setsockopt error ");
            close(sock[i]);
            exit(0);
        }
    }

    // /* followerとconnect */
    int connectserver_num = 0;
    printf("Start connect...\n");
    for (int i = 0; i < 4; i++)
    {
        int k = 0;
        connect(sock[i], (struct sockaddr *)&addr[i], sizeof(struct sockaddr_in));
        my_recv(sock[i], &k, sizeof(int) * 1);
        if (k == 1)
        {
            connectserver_num += k;
        }
    }
    printf("Finish connect with %dservers!\n", connectserver_num);

    struct AppendEntriesRPC_Argument *AERPC_A = malloc(sizeof(struct AppendEntriesRPC_Argument));
    struct AppendEntriesRPC_Result *AERPC_R = malloc(sizeof(struct AppendEntriesRPC_Result));
    struct AllServer_PersistentState *AS_PS = malloc(sizeof(struct AllServer_PersistentState));
    struct AllServer_VolatileState *AS_VS = malloc(sizeof(struct AllServer_VolatileState));
    struct Leader_VolatileState *L_VS = malloc(sizeof(struct Leader_VolatileState));

    // 初期設定
    AERPC_A->term = 1;
    AERPC_A->leaderID = 1;
    AERPC_A->prevLogIndex = 0;
    AERPC_A->prevLogTerm = 0;
    AERPC_A->leaderCommit = 0;

    AS_PS->currentTerm = 1;
    AS_PS->log[0].term = 0;
    for (int i = 0; i < ALL_ACCEPTED_ENTRIES; i++)
    {
        memset(AS_PS->log[i].entry, 0, sizeof(char) * STRING_MAX);
    }
    AS_PS->voteFor = 0;

    AS_VS->commitIndex = 0;
    AS_VS->LastAppliedIndex = 0;

    for (int i = 0; i < 4; i++)
    {
        L_VS->nextIndex[i] = 1; // (AERPC_A->leaderCommit=0) + 1
        L_VS->matchIndex[i] = 0;
    }

    char *str = malloc(sizeof(char) * STRING_MAX);
    int replicatelog_num;

    /* log記述用のファイル名 */
    make_logfile(argv[1]);

    // 時間記録用ファイル
    FILE *timerec;
    timerec = fopen("timerecord.txt", "w+");
    if (timerec == NULL)
    {
        printf("cannot open file\n");
        exit(1);
    }

    // printf("Input -> ");
    // scanf("%s", str);

    /* 接続済のソケットでデータのやり取り */
    // 今は受け取れるentryが有限
    for (int i = 1; i < (ALL_ACCEPTED_ENTRIES / ONCE_SEND_ENTRIES); i++)
    {
        printf("Input -> ");
        scanf("%s", str);
        /* followerに送る */
        /* AS_PSの更新 */
        // printf("%d", i);
        clock_gettime(CLOCK_MONOTONIC, &ts1);
        for (int num = 1; num < ONCE_SEND_ENTRIES; num++)
        {

            /* log[0]には入れない。log[1]から始める。　first index is 1*/
            // AERPC_A->entries[0] = str;
            strcpy(AS_PS->log[(i - 1) * (ONCE_SEND_ENTRIES - 1) + num].entry, str);
            AS_PS->log[(i - 1) * (ONCE_SEND_ENTRIES - 1) + num].term = AS_PS->currentTerm;

            // printf("AS_PS->log[%d].term = %d\n", (i - 1) * (NUM - 1) + num, AS_PS->log[(i - 1) * (NUM - 1) + num].term);
            // printf("AS_PS->log[%d].entry = %s\n\n", (i - 1) * (NUM - 1) + num, AS_PS->log[(i - 1) * (NUM - 1) + num].entry);

            /* logを書き込み */
            // printf("AS_PS->currentTerm = %d\n", AS_PS->currentTerm);
            // printf("AS_PS->voteFor = %d\n", AS_PS->voteFor);
            // printf(" AS_PS->log[%ld].term = %d\n", (i - 1) * (ONCE_SEND_ENTRIES - 1) + num, AS_PS->log[(i - 1) * (ONCE_SEND_ENTRIES - 1) + num].term);

            // printf("AS_PS->log[%ld].entry = %s\n\n", (i - 1) * (ONCE_SEND_ENTRIES - 1) + num, AS_PS->log[(i - 1) * (ONCE_SEND_ENTRIES - 1) + num].entry);
        }
        write_log(i, AS_PS);
        // read_log(i);

        /* AS_VSの更新 */
        // AS_VS->commitIndex = ; ここの段階での変更は起きない
        AS_VS->LastAppliedIndex += (ONCE_SEND_ENTRIES - 1); // = i

        replicatelog_num = AppendEntriesRPC(connectserver_num, sock, AERPC_A, AERPC_R, L_VS, AS_VS, AS_PS);

        if (replicatelog_num > (connectserver_num + 1) / 2)
        {
            printf("majority of servers replicated\n");
        }

        clock_gettime(CLOCK_MONOTONIC, &ts2);
        t = ts2.tv_sec - ts1.tv_sec + (ts2.tv_nsec - ts1.tv_nsec) / 1e9;

        fprintf(timerec, "%.4f\n", t);
        // fwrite(&t, sizeof(double), 1, timerec);
        printf("%.4f\n", t);
    }

    /* ソケット通信をクローズ */
    // for (int i = 0; i < 5; i++)
    // {
    //     close(sock[i]);
    // }

    exit(0);
}