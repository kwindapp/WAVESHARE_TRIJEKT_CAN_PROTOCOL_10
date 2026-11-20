/* Companion for RWB JANINE RENDER1 screens.c, LVGL 8.4.
 * Keep source function names for compatibility with main.cpp.
 * All positions and fonts are owned by screens.c.
 */
#include "mock_data.h"
#include <Arduino.h>
#include <lvgl.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "ui.h"
#include "screens.h"
#include "ecu_data.h"
extern objects_t objects;
extern "C" void rwb_set_tach_rpm(uint16_t rpm);

static bool changed(lv_obj_t *o,const char *s) {
    if(!o || !s) return false;
    if(strcmp(lv_label_get_text(o),s)==0) return false;
    lv_label_set_text(o,s); return true;
}

extern "C" void update_mock_rpm(void) {
    static uint32_t last=0;
    static int previous=-1;
    if(millis()-last<30) return;
    last=millis();
    if(!objects.label_rpm || !objects.arc_rpm_0_40 || !objects.bar_rpm_40_80) return;
    int rpm=constrain((int)ecu.rpm,0,8000);
    if(rpm==previous) return;
    previous=rpm;
    char buf[12]; snprintf(buf,sizeof(buf),"%04d",rpm);
    changed(objects.label_rpm,buf);
    rwb_set_tach_rpm((uint16_t)rpm);
    lv_arc_set_value(objects.arc_rpm_0_40,rpm>4000?4000:rpm);
    lv_bar_set_value(objects.bar_rpm_40_80,rpm>4000?rpm-4000:0,LV_ANIM_OFF);
    uint32_t c=rpm<=2000?0x00FF00:rpm<=4000?0xFFFF00:rpm<=6000?0xFFA500:0xFF0000;
    static uint32_t prevColor=0xFFFFFFFF;
    if(c!=prevColor) {
        prevColor=c;
        lv_obj_set_style_arc_color(objects.arc_rpm_0_40,lv_color_hex(c),LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(objects.bar_rpm_40_80,lv_color_hex(c),LV_PART_INDICATOR);
    }
}
extern "C" void update_mock_throttle(void) {
    static uint32_t last=0;
    if(millis()-last<200) return;
    last=millis();
    char buf[32]; snprintf(buf,sizeof(buf),"%.2f V",(double)ecu.batt);
    changed(objects.label_batt,buf);
}
extern "C" void update_mock_lambda(void) {
    /* Throttle intentionally not shown in the two-panel render. */
}
extern "C" void update_mock_motor_temp(void) {
    static uint32_t last=0;
    if(millis()-last<200) return;
    last=millis();
    float temp=ecu.motor;
    const char *color=temp<80.0f?"7DDFFF":temp<110.0f?"00FF00":temp<120.0f?"FFAA00":"FF0000";
    char buf[64]; snprintf(buf,sizeof(buf),"#%s %.1f C#",color,(double)temp);
    if(objects.label_throttle) {
        lv_label_set_recolor(objects.label_throttle,true);
        changed(objects.label_throttle,buf);
    }
}
