import glob
import time

Import("env")


def touch_before_upload(source, target, env):
    import serial

    port = env.subst("$UPLOAD_PORT") or ""
    if not port:
        candidates = glob.glob("/dev/cu.usbmodem*") + glob.glob("/dev/ttyACM*")
        if not candidates:
            return
        port = candidates[0]
    before = set(glob.glob("/dev/cu.usbmodem*") + glob.glob("/dev/ttyACM*"))
    try:
        s = serial.Serial(port, 1200, dsrdtr=False)
        s.dtr = False
        time.sleep(0.3)
        s.close()
    except Exception as exc:
        print("touch1200: skipped (%s)" % exc)
        return
    for _ in range(60):
        time.sleep(0.25)
        now = set(glob.glob("/dev/cu.usbmodem*") + glob.glob("/dev/ttyACM*"))
        fresh = now - before
        if fresh:
            new_port = sorted(fresh)[0]
            print("touch1200: device re-enumerated on %s" % new_port)
            env.Replace(UPLOAD_PORT=new_port)
            return
    print("touch1200: no new port appeared, trying %s" % port)


env.AddPreAction("upload", touch_before_upload)
