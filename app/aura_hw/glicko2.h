/****************************************************************************
 * aura_hw/glicko2.h
 * Glicko-2 动态难度（变式：固定波动率 σ，跳过 vol 迭代，
 * 保证嵌入式端确定性与低开销——方案 3.1 第五节 / 3.4）
 ****************************************************************************/

#ifndef AURA_HW_GLICKO2_H
#define AURA_HW_GLICKO2_H

typedef struct
{
  float rating;       /* μ，初始 1500 */
  float rd;           /* φ，初始 350 */
  float volatility;   /* σ，固定 0.06 */
} glicko2_t;

void glicko2_init(glicko2_t *p);

/* score: 0.0~1.0（本局准确率）；opp_rating/opp_rd: 对手（谱面难度）评分 */
void glicko2_update(glicko2_t *p, float opp_rating, float opp_rd,
                    float score);

/* μ → 谱面密度档位 1~5（难度-密度映射表，线性分档） */
int  glicko2_density_level(const glicko2_t *p);

#endif /* AURA_HW_GLICKO2_H */
