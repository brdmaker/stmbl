# Components built into the host simulator and the stmbl_sim module.
# Portable control components: the V4 SHARED_COMPS set from ../base.mak. 'term'
# (the USB console / scope channel) is included via the host CDC shim in shim/.
COMPS = \
  sim svm vel rev hal_test ypid fault pid spid pe \
  pmsm_limits pmsm_ttc dc_limits dc_ttc acim_ttc uvw fanuc fb_switch reslimit \
  iit vel_int linrev psi stp uf2 ramp scale idx_home move not and or jog \
  velbuf avg mux veltopos debounce pos_filter rl mad sensorless field gain \
  rlpsij veltime mpid fmove home en th motsim term \
  idm idpmsm ids iddc acvf

# Simulated drive hardware: stand-ins for the src/comps components the drive
# config templates link to (same names and pins), plus the unchanged conf.
HW_COMPS = hv enc_fb sserial adc io

