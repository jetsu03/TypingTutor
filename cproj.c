#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <ctype.h>
#include <sqlite3.h>
#include <time.h>
#define MAX_PARAGRAPH_LENGTH 200
#define MAX_FILE_LINE_LENGTH 200
#define MAX_USERNAME_LENGTH 50
typedef struct {
    double accuracy;
    double elapsedTime;
    char paragraph[MAX_PARAGRAPH_LENGTH];
} TypingStats;
typedef struct {
    char username[MAX_USERNAME_LENGTH];
    int attempts;
    double averageTime;
    double averageAccuracy;
} User;
char* getRandomParagraph(FILE* file) {
    char line[MAX_FILE_LINE_LENGTH];
    int numParagraphs = 0;

    while (fgets(line, sizeof(line), file) != NULL) {
        if (line[0] != '\n') numParagraphs++;
    }

    if (numParagraphs == 0) {
        fprintf(stderr, "Error: No paragraphs found in the file.\n");
        exit(EXIT_FAILURE);
    }

    fseek(file, 0, SEEK_SET);
    int randomIndex = rand() % numParagraphs;

    for (int i = 0; i < randomIndex; i++) {
        if (fgets(line, sizeof(line), file) == NULL) {
            fprintf(stderr, "Error reading paragraph from file.\n");
            exit(EXIT_FAILURE);
        }
    }

    line[strcspn(line, "\n")] = '\0'; // Remove trailing newline

    char* paragraph = strdup(line);
    if (paragraph == NULL) {
        perror("Memory allocation error");
        exit(EXIT_FAILURE);
    }

    return paragraph;
}

void printTypingStats(double elapsedTime, const char* input, const char* correctText, TypingStats* stats) {
    int correctCount = 0;
    int minLen = strlen(correctText) < strlen(input) ? strlen(correctText) : strlen(input);

    for (int i = 0; i < minLen; i++) {
        if (correctText[i] == input[i]) correctCount++;
    }

    stats->accuracy = (double)correctCount / strlen(correctText) * 100;
    stats->elapsedTime = elapsedTime;
    strncpy(stats->paragraph, correctText, MAX_PARAGRAPH_LENGTH);
}

void displayTypingStats(const TypingStats* stats) {
    printf("\nTyping Stats for Current Attempt:\n");
    printf("--------------------------------------------------------\n");
    printf("Accuracy: %.2f%%\n", stats->accuracy);
    printf("Time Taken: %.2f seconds\n", stats->elapsedTime);
    printf("--------------------------------------------------------\n");
}

void login(User* user) {
    printf("Enter your username: ");
    if (fgets(user->username, sizeof(user->username), stdin) == NULL) {
        perror("Error reading username");
        exit(EXIT_FAILURE);
    }
    user->username[strcspn(user->username, "\n")] = '\0'; // Remove trailing newline
}

void logout() {
    printf("\nLogging out...\n");
}

void createTableIfNotExists(sqlite3* db) {
    const char* createTableSQL =
        "CREATE TABLE IF NOT EXISTS user_stats ("
        "user TEXT PRIMARY KEY, "
        "attempts INTEGER, "
        "average_time REAL, "
        "accuracy REAL, "
        "rank_score REAL);";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, createTableSQL, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare table creation statement: %s\n", sqlite3_errmsg(db));
        exit(EXIT_FAILURE);
    }

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Failed to execute table creation statement: %s\n", sqlite3_errmsg(db));
        exit(EXIT_FAILURE);
    }

    sqlite3_finalize(stmt);
}

void updateUserStats(sqlite3* db, const User* user) {
    sqlite3_stmt* stmt;
    char sql[256];

    double rankScore = user->averageAccuracy - 0.5 * user->averageTime;

    snprintf(sql, sizeof(sql),
        "INSERT OR REPLACE INTO user_stats (user, attempts, average_time, accuracy, rank_score) "
        "VALUES (?, ?, ?, ?, ?);");

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        exit(EXIT_FAILURE);
    }

    sqlite3_bind_text(stmt, 1, user->username, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, user->attempts);
    sqlite3_bind_double(stmt, 3, user->averageTime);
    sqlite3_bind_double(stmt, 4, user->averageAccuracy);
    sqlite3_bind_double(stmt, 5, rankScore);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
    }

    sqlite3_finalize(stmt);
}

void displayRankings(sqlite3* db) {
    const char* query = "SELECT user, attempts, average_time, accuracy, rank_score FROM user_stats ORDER BY rank_score DESC;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare ranking query: %s\n", sqlite3_errmsg(db));
        return;
    }

    printf("\n--- Ranking Table ---\n");
    printf("User\tAttempts\tAverage Time\tAccuracy\tRank Score\n");
    printf("---------------------------------------------------------------\n");

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* username = sqlite3_column_text(stmt, 0);
        int attempts = sqlite3_column_int(stmt, 1);
        double averageTime = sqlite3_column_double(stmt, 2);
        double accuracy = sqlite3_column_double(stmt, 3);
        double rankScore = sqlite3_column_double(stmt, 4);

        printf("%s\t%d\t\t%.2f seconds\t%.2f%%\t\t%.2f\n", username, attempts, averageTime, accuracy, rankScore);
    }

    printf("---------------------------------------------------------------\n");

    sqlite3_finalize(stmt);
}

void processAttempts(FILE* file, User* user, sqlite3* db) {
    srand((unsigned int)time(NULL));

    printf("Welcome to Typing Tutor, %s!\n", user->username);

    char input[MAX_PARAGRAPH_LENGTH];
    TypingStats currentAttempt;
    user->attempts = 0;
    user->averageTime = 0;
    user->averageAccuracy = 0;

    while (1) {
        char* currentParagraph = getRandomParagraph(file);

        printf("\nType the following paragraph:\n%s\n", currentParagraph);

        struct timeval startTime, endTime;
        gettimeofday(&startTime, NULL);

        printf("Your input:\n");
        fflush(stdout);

        if (fgets(input, sizeof(input), stdin) == NULL) {
            perror("Error reading input");
            free(currentParagraph);
            exit(EXIT_FAILURE);
        }

        gettimeofday(&endTime, NULL);

        double elapsedTime = (endTime.tv_sec - startTime.tv_sec) + (endTime.tv_usec - startTime.tv_usec) / 1000000.0;
        input[strcspn(input, "\n")] = '\0'; // Remove trailing newline

        printTypingStats(elapsedTime, input, currentParagraph, &currentAttempt);
        displayTypingStats(&currentAttempt);

        user->attempts++;
        user->averageTime = (user->averageTime * (user->attempts - 1) + elapsedTime) / user->attempts;
        user->averageAccuracy = (user->averageAccuracy * (user->attempts - 1) + currentAttempt.accuracy) / user->attempts;

        updateUserStats(db, user);

        free(currentParagraph);

        printf("\nDo you want to continue? (y/n): ");
        char choice[3];
        if (fgets(choice, sizeof(choice), stdin) == NULL) {
            perror("Error reading choice");
            exit(EXIT_FAILURE);
        }

        if (tolower(choice[0]) != 'y') {
            logout();
            displayRankings(db);
            printf("\nThanks for using Typing Tutor!\n");
            break;
        }
    }

    fclose(file);
}
int main() {
    sqlite3* db;
    if (sqlite3_open("cproj.db", &db) != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    createTableIfNotExists(db);

    FILE* file = fopen("paragraphs.txt", "r");
    if (file == NULL) {
        perror("Error opening file 'paragraphs.txt'");
        sqlite3_close(db);
        return 1;
    }

    User user;
    login(&user);
    processAttempts(file, &user, db);

    sqlite3_close(db);
    return 0;
}

