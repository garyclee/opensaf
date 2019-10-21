#ifndef LCK_LCKD_GLD_CLM_H_
#define LCK_LCKD_GLD_CLM_H_

#include "lck/lckd/gld.h"

#ifdef __cplusplus
extern "C" {
#endif

void gld_clm_reinit_bg(GLSV_GLD_CB *);
SaAisErrorT gld_clm_init(GLSV_GLD_CB *);
SaAisErrorT gld_clm_deinit(GLSV_GLD_CB *);

#ifdef __cplusplus
}
#endif

#endif  // LCK_LCKND_GLND_CLM_H_
