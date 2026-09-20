/****************************************************************************
 * aura_hw/glicko2.c
 ****************************************************************************/

#include "glicko2.h"

#include <math.h>

#define GLICKO_Q        173.7178f   /* Glicko-2 标度换算常数 */
#define GLICKO_PI_SQ    9.8696f

void glicko2_init(glicko2_t *p)
{
  p->rating     = 1500.0f;
  p->rd         = 350.0f;
  p->volatility = 0.06f;
}

static float g_fun(float phi)
{
  return 1.0f / sqrtf(1.0f + 3.0f * phi * phi / GLICKO_PI_SQ);
}

void glicko2_update(glicko2_t *p, float opp_rating, float opp_rd,
                    float score)
{
  float mu        = (p->rating - 1500.0f) / GLICKO_Q;
  float phi       = p->rd / GLICKO_Q;
  float mu_j      = (opp_rating - 1500.0f) / GLICKO_Q;
  float phi_j     = opp_rd / GLICKO_Q;
  float g         = g_fun(phi_j);
  float e         = 1.0f / (1.0f + expf(-g * (mu - mu_j)));
  float v         = 1.0f / (g * g * e * (1.0f - e));
  float phi_star;
  float phi_new;
  float mu_new;

  /* 变式：σ 固定，不做波动率迭代（单局更新，保证确定性） */
  phi_star = sqrtf(phi * phi + p->volatility * p->volatility);
  phi_new  = 1.0f / sqrtf(1.0f / (phi_star * phi_star) + 1.0f / v);
  mu_new   = mu + phi_new * phi_new * g * (score - e);

  p->rating = mu_new * GLICKO_Q + 1500.0f;
  p->rd     = phi_new * GLICKO_Q;
}

int glicko2_density_level(const glicko2_t *p)
{
  if (p->rating < 1300.0f) return 1;
  if (p->rating < 1500.0f) return 2;
  if (p->rating < 1700.0f) return 3;
  if (p->rating < 1900.0f) return 4;
  return 5;
}
