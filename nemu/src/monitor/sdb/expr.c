/***************************************************************************************
* Copyright (c) 2014-2024 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include <isa.h>

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <regex.h>

enum {
  TK_LEFT_PAREN = '(',
  TK_RIGHT_PAREN = ')',
  TK_NOTYPE = 256, TK_EQ,

  /* TODO: Add more token types */
  TK_NUMBER,
  TK_NEGATIVE,
};

static struct rule {
  const char *regex;
  int token_type;
} rules[] = {

  /* TODO: Add more rules.
   * Pay attention to the precedence level of different rules.
   */

  {"[0-9]+", TK_NUMBER},
  {" +", TK_NOTYPE},    // spaces
  {"\\+", '+'},         // plus
  {"-", '-'},           // minus
  {"\\*", '*'},           // multiply
  {"/", '/'},           // divide
  {"\\(", '('},           // left-bracket
  {"\\)", ')'},           // right-bracket
  {"==", TK_EQ},        // equal
};

#define NR_REGEX ARRLEN(rules)

static regex_t re[NR_REGEX] = {};

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i ++) {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0) {
      regerror(ret, &re[i], error_msg, 128);
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }
}

typedef struct token {
  int type;
  char str[32];
} Token;

static Token tokens[65535] __attribute__((used)) = {};
static int nr_token __attribute__((used))  = 0;

static bool make_token(char *e) {
  int position = 0;
  int i;
  regmatch_t pmatch;

  nr_token = 0;

  while (e[position] != '\0') {
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i ++) {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
            i, rules[i].regex, position, substr_len, substr_len, substr_start);

        position += substr_len;

        /* TODO: Now a new token is recognized with rules[i]. Add codes
         * to record the token in the array `tokens'. For certain types
         * of tokens, some extra actions should be performed.
         */

        if (rules[i].token_type == TK_NOTYPE) {
          break;
        }
        if (nr_token >= sizeof(tokens)/sizeof(tokens[0])) {
          printf("Error: too many tokens\n");
          return false;
        }
        if (substr_len >= sizeof(tokens[nr_token].str)) {
          printf("Error: token too long\n");
          return false;
        }

        tokens[nr_token].type = rules[i].token_type;
        strncpy(tokens[nr_token].str, substr_start, substr_len);
        tokens[nr_token].str[substr_len] = 0;

        if (rules[i].token_type == '-') {
          if (nr_token == 0 || 
            tokens[nr_token-1].type == '+' ||
            tokens[nr_token-1].type == '-' ||
            tokens[nr_token-1].type == '*' ||
            tokens[nr_token-1].type == '/' ||
            tokens[nr_token-1].type == TK_NEGATIVE
          ) {
            tokens[nr_token].type = TK_NEGATIVE;
          }
        }

        nr_token++;

        break;
      }
    }

    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }

  return true;
}

static int find_right_paren(int p, int q) {
  int cnt = 0;
  for (int i = p; i <= q; i++) {
    if (tokens[i].type == TK_LEFT_PAREN) {
      cnt++;
    } else if (tokens[i].type == TK_RIGHT_PAREN) {
      if (--cnt == 0) {
        return i;
      }
    }
  }
  return -1;
}

static int find_main_op(int p, int q) {
  int priority = 1;
  int main_op = -1;
  int level = 0;
  for (int i = p; i <= q; i++) {
    if (tokens[i].type == TK_LEFT_PAREN) {
      level++;
      continue;
    } else if (tokens[i].type == TK_RIGHT_PAREN) {
      level--;
      continue;
    }
    if (level > 0) {
      continue;
    }
    if (tokens[i].type == '+' || tokens[i].type == '-') {
      main_op = i;
      priority = 0;
    } else if (priority >= 1 && (tokens[i].type == '*' || tokens[i].type == '/')) {
      main_op = i;
      priority = 1;
    }
  }
  return main_op;
}

static int eval(int p, int q, bool *success) {
  if (p > q) {
    printf("Error: format error\n");
    goto ERROR_RETURN;
  } else if (p == q) {
    if (tokens[p].type != TK_NUMBER) {
      printf("Error: number is expected\n");
      goto ERROR_RETURN;
    }

    *success = true;
    return atoi(tokens[p].str);
  } else if (tokens[p].type == TK_LEFT_PAREN) {
    int right_paren_pos = find_right_paren(p, q);
    if (right_paren_pos == -1) {
      printf("Error: parentheses not match\n");
      goto ERROR_RETURN;
    }
    if (right_paren_pos == q) {
      return eval(p+1, q-1, success);
    }
  }

  int main_op = find_main_op(p, q);
  if (main_op == -1) {
    if (tokens[p].type == TK_NEGATIVE) {
      return -eval(p+1, q, success);
    }
    printf("Error: could not find main op\n");
    goto ERROR_RETURN;
  }

  int val1 = 0;
  if (tokens[main_op].type == '-' && p == main_op) {
    val1 = 0;
  } else {
    val1 = eval(p, main_op-1, success);
    if (!*success) goto ERROR_RETURN;
  }
  int val2 = eval(main_op+1, q, success);
  if (!*success) goto ERROR_RETURN;

  *success = true;
  switch (tokens[main_op].type) {
    case '+': return val1+val2;
    case '-': return val1-val2;
    case '*': return val1*val2;
    case '/':
      if (val2 == 0) {
        printf("Error: divide 0\n");
        goto ERROR_RETURN;
      }
      return val1/val2;
    default:
      printf("Error: unexpected token type: %d\n", tokens[main_op].type);
      goto ERROR_RETURN;
  }

ERROR_RETURN:
  *success = false;
  return 0;
}

word_t expr(char *e, bool *success) {
  if (!make_token(e)) {
    *success = false;
    return 0;
  }

  return eval(0, nr_token-1, success);
}

#define MAX_LINE_LENGTH 65535
int check_expr_file() {
  FILE *file;
  char line[MAX_LINE_LENGTH];
  unsigned int expected;
  char expression[MAX_LINE_LENGTH];

  file = fopen("input", "r");
  if (file == NULL) {
      perror("cannot open file");
      return EXIT_FAILURE;
  }

  int total_cnt = 0, success_cnt = 0;

  while (fgets(line, sizeof(line), file)) {
      if (sscanf(line, "%u %[^\n]", &expected, expression) == 2) {
        total_cnt++;
        bool success = false;
        word_t result = expr(expression, &success);
        if (!success) {
          printf("Error: not success. [%s]\n", line);
        }
        else if (result != expected) {
          printf("Error: not equal: [%s] != %u\n", line, result);
        } else {
          success_cnt++;
        }
      } else {
        printf("Error: wrong format: [%s]\n", line);
        exit(1);
      }
  }

  printf("Total count = %d. Success count = %d\n", total_cnt, success_cnt);

  fclose(file);
  return 0;
}
