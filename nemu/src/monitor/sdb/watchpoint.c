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

#include "sdb.h"

#define NR_WP 32

static WP wp_pool[NR_WP] = {};
static WP head, free_;

void init_wp_pool() {
  int i;
  for (i = 0; i < NR_WP; i ++) {
    wp_pool[i].NO = i;
    wp_pool[i].next = (i == NR_WP - 1 ? NULL : &wp_pool[i + 1]);
  }

  head.next = NULL;
  free_.next = wp_pool;
}

WP* new_wp() {
  if (free_.next == NULL) return NULL;
  WP* ret = free_.next;
  free_.next = ret->next;
  ret->next = head.next;
  head.next = ret;
  return ret;
}

// wp_id starts from 1.
bool free_wp(int wp_id) {
  if (wp_id < 0 || wp_id >= sizeof(wp_pool) / sizeof(wp_pool[0])) {
    return false;
  }

  WP *wp = &wp_pool[wp_id];

  WP *prev = &head;
  while (prev && prev->next != wp) {
    prev = prev->next;
  }

  if (prev == NULL) {
    return false;
  }

  prev->next = wp->next;
  wp->next = free_.next;
  free_.next = wp;
  return true;
}

void print_wps() {
  WP *p = head.next;
  printf("No.\tWhat\n");
  while (p) {
    printf("%d\t%s\n", p->NO, p->str);
    p = p->next;
  }
}

bool evaluate_all_watchpoint() {
  WP *wp = head.next;
  while (wp) {
    bool success = false;
    word_t val = expr(wp->str, &success);
    if (success && val != wp->val) {
      printf("Watchpoint triggered. no. = %d, expr = %s, old_val = %u, new_val = %u\n", wp->NO, wp->str, wp->val, val);
      wp->val = val;
      return true;
    }

    wp = wp->next;
  }

  return false;
}