#ifndef LIGHT_CURTAIN_H
#define LIGHT_CURTAIN_H

// bitvector containing 1=emitter/detector not blocked 0=blocked
extern uint32_t Emitter_Vector;

int16_t GetLightCurtainData      (void);
void    GetLightCurtainData_Init (void);
void    LightCurtainSync         (void);

#endif
