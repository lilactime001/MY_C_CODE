#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* =========================================
 * 常量与枚举
 * ========================================= */
#define MAX_SKILLS 4
#define MAX_NAME   20

 // 技能效果类型
typedef enum {
    SKILL_DAMAGE,      // 纯伤害
    SKILL_BLEED,       // 伤害 + 挂流血
    SKILL_HEAL,        // 回血
    SKILL_BUFF_DEF     // 临时加防（持续本回合）
} SkillEffect;

typedef struct {
    char name[MAX_NAME];
    int  mp_cost;
    int  cooldown;       // 冷却回合数
    int  cur_cd;         // 当前剩余冷却
    SkillEffect effect;
    int  power;          // 伤害倍率(%) 或 回血量
} Skill;

typedef struct {
    char name[MAX_NAME];
    int hp, max_hp;
    int mp, max_mp;
    int atk;
    int crit_rate;
    int defense;
    int speed;
    int bleed_turns;     // 流血剩余回合
    int def_buff;        // 临时防御加成（本回合有效）
    Skill skills[MAX_SKILLS];
    int skill_count;
} Entity;

/* =========================================
 * 工具函数
 * ========================================= */
void print_bar(const char* label, int cur, int max) {
    int len = 20;
    int filled = max > 0 ? cur * len / max : 0;
    printf("%s [", label);
    for (int i = 0; i < len; i++) putchar(i < filled ? '#' : '.');
    printf("] %d/%d\n", cur, max);
}

void print_status(Entity* e) {
    printf("┌─ %s ─────────────\n", e->name);
    print_bar("HP", e->hp, e->max_hp);
    print_bar("MP", e->mp, e->max_mp);
    printf("  ATK:%d  DEF:%d  SPD:%d  CRIT:%d%%\n",
        e->atk, e->defense + e->def_buff, e->speed, e->crit_rate);
    if (e->bleed_turns > 0)
        printf("  ?? 流血中（剩余 %d 回合）\n", e->bleed_turns);
    printf("└──────────────────\n");
}

/* =========================================
 * 核心战斗逻辑
 * ========================================= */
int calc_damage(Entity* attacker, Entity* defender, int power_percent, int* out_crit) {
    int is_crit = (rand() % 100) < attacker->crit_rate;
    if (out_crit) *out_crit = is_crit;

    int base = attacker->atk * power_percent / 100;
    if (is_crit) base *= 2;

    int final = base - (defender->defense + defender->def_buff);
    if (final < 1) final = 1;
    return final;
}

void apply_damage(Entity* target, int dmg) {
    target->hp -= dmg;
    if (target->hp < 0) target->hp = 0;
}

void apply_bleed(Entity* target) {
    target->bleed_turns = 3;
    printf("?? %s 进入【流血】状态（3 回合，每回合 5 点）！\n", target->name);
}

void tick_bleed(Entity* target) {
    if (target->bleed_turns > 0 && target->hp > 0) {
        printf("?? 【流血】%s 受到 5 点伤害（剩余 %d 回合）。\n",
            target->name, target->bleed_turns);
        apply_damage(target, 5);
        target->bleed_turns--;
    }
}

/* =========================================
 * 行动执行：普通攻击
 * ========================================= */
void action_attack(Entity* attacker, Entity* defender) {
    int crit = 0;
    int dmg = calc_damage(attacker, defender, 100, &crit);
    printf("????  %s 发动普通攻击！%s", attacker->name, crit ? "??【暴击】" : "");
    printf("造成 %d 点伤害。\n", dmg);
    apply_damage(defender, dmg);
    if (crit && defender->hp > 0) apply_bleed(defender);
    printf("   %s 剩余 HP：%d/%d\n", defender->name, defender->hp, defender->max_hp);
}

/* =========================================
 * 行动执行：使用技能
 * ========================================= */
void action_skill(Entity* attacker, Entity* defender, int idx) {
    Skill* s = &attacker->skills[idx];

    // 扣蓝、进冷却
    attacker->mp -= s->mp_cost;
    s->cur_cd = s->cooldown;

    printf("?? %s 使用【%s】！\n", attacker->name, s->name);

    switch (s->effect) {
    case SKILL_DAMAGE: {
        int crit = 0;
        int dmg = calc_damage(attacker, defender, s->power, &crit);
        printf("   造成 %d 点伤害%s。\n", dmg, crit ? "（暴击）" : "");
        apply_damage(defender, dmg);
        if (crit && defender->hp > 0) apply_bleed(defender);
        break;
    }
    case SKILL_BLEED: {
        int crit = 0;
        int dmg = calc_damage(attacker, defender, s->power, &crit);
        printf("   造成 %d 点伤害。\n", dmg);
        apply_damage(defender, dmg);
        if (defender->hp > 0) apply_bleed(defender); // 必定流血
        break;
    }
    case SKILL_HEAL: {
        int heal = s->power;
        attacker->hp += heal;
        if (attacker->hp > attacker->max_hp) attacker->hp = attacker->max_hp;
        printf("   ?? 回复 %d 点 HP。\n", heal);
        break;
    }
    case SKILL_BUFF_DEF: {
        attacker->def_buff += s->power;
        printf("   ????  防御力提升 %d（持续本回合）。\n", s->power);
        break;
    }
    }

    if (defender->hp > 0)
        printf("   %s 剩余 HP：%d/%d\n", defender->name, defender->hp, defender->max_hp);
}

/* =========================================
 * 回合开始/结束处理
 * ========================================= */
void turn_start(Entity* e) {
    e->def_buff = 0; // 清空临时防御
    for (int i = 0; i < e->skill_count; i++) {
        if (e->skills[i].cur_cd > 0) e->skills[i].cur_cd--;
    }
}

/* =========================================
 * 玩家输入
 * ========================================= */
int player_choose_action(Entity* player) {
    while (1) {
        printf("\n【你的回合】请选择行动：\n");
        printf("  1. 普通攻击\n");
        printf("  2. 使用技能\n");
        printf("  3. 防御（本回合防御 +50%%）\n");
        printf("  4. 逃跑\n");
        printf("> ");

        int choice;
        if (scanf("%d", &choice) != 1) {
            while (getchar() != '\n');
            continue;
        }

        if (choice == 1) return 1;

        if (choice == 2) {
            printf("\n技能列表：\n");
            for (int i = 0; i < player->skill_count; i++) {
                Skill* s = &player->skills[i];
                printf("  %d. %-10s MP:%2d  冷却:%d/%d  %s\n",
                    i + 1, s->name, s->mp_cost, s->cur_cd, s->cooldown,
                    s->cur_cd > 0 ? "（冷却中）" :
                    (player->mp < s->mp_cost ? "（MP不足）" : ""));
            }
            printf("  0. 返回\n> ");
            int sc;
            if (scanf("%d", &sc) != 1) { while (getchar() != '\n'); continue; }
            if (sc == 0) continue;
            if (sc < 1 || sc > player->skill_count) continue;
            Skill* s = &player->skills[sc - 1];
            if (s->cur_cd > 0) { printf("?? 技能冷却中！\n"); continue; }
            if (player->mp < s->mp_cost) { printf("?? MP 不足！\n"); continue; }
            return 1 + sc; // 返回 2~5 代表技能 0~3
        }

        if (choice == 3) return 100;
        if (choice == 4) return 999;
        printf("无效输入，请重试。\n");
    }
}

/* =========================================
 * 执行一个实体的一回合行动
 * 返回 1 表示主动逃跑
 * ========================================= */
int do_turn(Entity* actor, Entity* target, int is_player) {
    int action;
    if (is_player) {
        action = player_choose_action(actor);
    }
    else {
        // 简单 AI：MP 够且技能可用就放技能，否则普攻
        action = 1;
        for (int i = 0; i < actor->skill_count; i++) {
            Skill* s = &actor->skills[i];
            if (s->cur_cd == 0 && actor->mp >= s->mp_cost) {
                if (s->effect == SKILL_HEAL && actor->hp > actor->max_hp / 2) continue;
                action = 2 + i;
                break;
            }
        }
    }

    if (action == 999) {
        printf("?? %s 试图逃跑……\n", actor->name);
        if (rand() % 100 < 50) {
            printf("?? 逃跑成功！\n");
            return 1;
        }
        printf("?? 逃跑失败，浪费了本回合！\n");
        return 0;
    }

    if (action == 100) {
        actor->def_buff += 5; // 防御加 5
        printf("????  %s 摆出防御姿态（防御 +5）。\n", actor->name);
        return 0;
    }

    if (action == 1) {
        action_attack(actor, target);
    }
    else {
        action_skill(actor, target, action - 2);
    }
    return 0;
}

/* =========================================
 * 初始化角色与技能
 * ========================================= */
void init_player(Entity* p) {
    strcpy(p->name, "勇者");
    p->max_hp = 120; p->hp = 120;
    p->max_mp = 50;  p->mp = 50;
    p->atk = 20;
    p->crit_rate = 25;
    p->defense = 8;
    p->speed = 15;
    p->bleed_turns = 0;
    p->def_buff = 0;
    p->skill_count = 3;

    p->skills[0] = (Skill){ "烈风斩", 10, 2, 0, SKILL_DAMAGE, 180 };
    p->skills[1] = (Skill){ "撕裂之刃", 15, 3, 0, SKILL_BLEED, 120 };
    p->skills[2] = (Skill){ "治愈术", 12, 4, 0, SKILL_HEAL, 40 };
}

void init_enemy(Entity* e) {
    strcpy(e->name, "哥布林战士");
    e->max_hp = 90; e->hp = 90;
    e->max_mp = 30; e->mp = 30;
    e->atk = 16;
    e->crit_rate = 15;
    e->defense = 5;
    e->speed = 12;
    e->bleed_turns = 0;
    e->def_buff = 0;
    e->skill_count = 2;

    e->skills[0] = (Skill){ "猛击", 8, 2, 0, SKILL_DAMAGE, 160 };
    e->skills[1] = (Skill){ "毒牙", 10, 3, 0, SKILL_BLEED, 100 };
}

/* =========================================
 * 主循环
 * ========================================= */
int main() {
    srand((unsigned int)time(NULL));

    Entity player, enemy;
    init_player(&player);
    init_enemy(&enemy);

    printf("????  战斗开始！【%s】出现了！\n", enemy.name);
    printf("   勇者速度:%d  敌人速度:%d\n\n", player.speed, enemy.speed);

    int turn = 1;
    while (player.hp > 0 && enemy.hp > 0) {
        printf("\n========== 第 %d 回合 ==========\n", turn);
        print_status(&player);
        print_status(&enemy);

        // 回合开始：清临时防御、扣冷却
        turn_start(&player);
        turn_start(&enemy);

        // 按速度排序
        Entity* first = (enemy.speed > player.speed) ? &enemy : &player;
        Entity* second = (first == &player) ? &enemy : &player;
        int first_is_player = (first == &player);

        // 先手行动
        int fled = do_turn(first, second, first_is_player);
        if (fled) { printf("战斗结束（逃跑）。\n"); return 0; }

        if (second->hp > 0) {
            printf("\n");
            fled = do_turn(second, first, !first_is_player);
            if (fled) { printf("战斗结束（逃跑）。\n"); return 0; }
        }
        else {
            printf("?? %s 被击倒，无法反击！\n", second->name);
        }

        // 回合结束：流血结算
        printf("\n--- 回合结束结算 ---\n");
        tick_bleed(&player);
        tick_bleed(&enemy);

        if (player.hp == 0 || enemy.hp == 0) break;
        turn++;
    }

    printf("\n================================\n");
    if (player.hp > 0 && enemy.hp == 0)
        printf("?? 战斗胜利！你击败了 %s！\n", enemy.name);
    else if (player.hp == 0)
        printf("?? 战斗失败……你被 %s 击败了。\n", enemy.name);

    return 0;
}
