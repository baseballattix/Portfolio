import os
os.environ["QT_OPENGL"] = "angle"
os.environ["QTWEBENGINE_CHROMIUM_FLAGS"] = "--enable-features=Metal"
os.environ["PYOPENGL_PLATFORM"] = "qt5"

import math
import time
import csv
import serial
import numpy as np
 
from pyqtgraph.Qt import QtCore, QtWidgets, QtGui
import pyqtgraph.opengl as gl
import pyqtgraph as pg


# ---------------- CONFIG ----------------
PORT = "/dev/tty.usbmodem1101"
BAUD = 115200
MAX_POINTS = 5000
USE_BARO_ALT = False
LOG_FILE = "rx_telemetry_log.csv"
FLUSH_EVERY = 20

# Expected RX CSV line:
# CSV,seq,t_ms,lat,lon,alt_gnss_m,pres_hPa,alt_baro_m,temp_C,sats,
# ax_g,ay_g,az_g,gx_dps,gy_dps,gz_dps,mph,event
IDX = {
    "seq": 1,
    "t_ms": 2,
    "lat": 3,
    "lon": 4,
    "alt_gnss_m": 5,
    "pres_hPa": 6,
    "alt_baro_m": 7,
    "temp_C": 8,
    "sats": 9,
    "ax_g": 10,
    "ay_g": 11,
    "az_g": 12,
    "gx_dps": 13,
    "gy_dps": 14,
    "gz_dps": 15,
    "mph": 16,
    "event": 17,
}
EXPECTED_FIELDS = 18

AX_LEN_M = 1000.0
GRID_HALF_SIZE_M = 500.0
GRID_STEP_M = 50.0
PLOT_HZ = 15.0


def ll_to_local_xy(lat0: float, lon0: float, lat: float, lon: float):
    m_lat = 111320.0
    m_lon = 111320.0 * math.cos(math.radians(lat0))
    x_east = (lon - lon0) * m_lon
    y_north = (lat - lat0) * m_lat
    return x_east, y_north


def build_rect_grid(half_size_m: float, step_m: float):
    lines = []
    colors = []
    major_every = 5
    n = int(half_size_m // step_m)
    coords = [i * step_m for i in range(-n, n + 1)]

    for i, x in enumerate(coords):
        lines.append([[-half_size_m, x, 0.0], [half_size_m, x, 0.0]])
        is_major = (i % major_every == 0)
        colors.append((1, 1, 1, 0.18) if is_major else (1, 1, 1, 0.08))

    for i, y in enumerate(coords):
        lines.append([[y, -half_size_m, 0.0], [y, half_size_m, 0.0]])
        is_major = (i % major_every == 0)
        colors.append((1, 1, 1, 0.18) if is_major else (1, 1, 1, 0.08))

    pos = np.array(lines, dtype=float).reshape(-1, 3)

    seg_colors = []
    for c in colors:
        seg_colors.extend([c, c])
    color = np.array(seg_colors, dtype=float)

    return pos, color


def set_status_label(label, text, color):
    label.setText(text)
    label.setStyleSheet(
        f"font-size: 22px; "
        f"font-family: monospace; "
        f"color: {color}; "
        f"background-color: black;"
    )


class AttitudeWidget(QtWidgets.QWidget):
    """
    Simple pitch/roll paint window.
    Y axis controls nose/pitch.
    """
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Orientation")
        self.resize(320, 320)
        self.setStyleSheet("background-color: black;")
        self.roll_deg = 0.0
        self.pitch_deg = 0.0

    def set_attitude(self, roll_deg, pitch_deg):
        self.roll_deg = float(roll_deg)
        self.pitch_deg = float(pitch_deg)
        self.update()

    def paintEvent(self, ev):
        p = QtGui.QPainter(self)

        try:
            try:
                p.setRenderHint(QtGui.QPainter.RenderHint.Antialiasing, True)
            except Exception:
                p.setRenderHint(QtGui.QPainter.Antialiasing, True)

            w = self.width()
            h = self.height()
            cx = w / 2.0
            cy = h / 2.0
            radius = min(w, h) * 0.40

            pen = QtGui.QPen(QtGui.QColor(200, 200, 200))
            pen.setWidth(2)
            p.setPen(pen)
            p.drawEllipse(QtCore.QPointF(cx, cy), radius, radius)

            p.save()
            p.translate(cx, cy)
            p.rotate(-self.roll_deg)

            pitch_px = self.pitch_deg * 2.0

            pen2 = QtGui.QPen(QtGui.QColor(255, 255, 255))
            pen2.setWidth(3)
            p.setPen(pen2)
            p.drawLine(
                QtCore.QPointF(-radius - 40, pitch_px),
                QtCore.QPointF(radius + 40, pitch_px),
            )
            p.restore()

            pen3 = QtGui.QPen(QtGui.QColor(255, 255, 0))
            pen3.setWidth(2)
            p.setPen(pen3)
            p.drawLine(int(cx - 18), int(cy), int(cx + 18), int(cy))
            p.drawLine(int(cx), int(cy - 18), int(cx), int(cy + 18))

        finally:
            p.end()


def main():
    ser = serial.Serial(PORT, BAUD, timeout=0.2)
    time.sleep(0.5)

    log = open(LOG_FILE, "w", newline="")
    writer = csv.writer(log)
    writer.writerow([
        "seq",
        "t_ms",
        "lat",
        "lon",
        "alt_gnss_m",
        "pres_hPa",
        "alt_baro_m",
        "temp_C",
        "sats",
        "ax_g",
        "ay_g",
        "az_g",
        "gx_dps",
        "gy_dps",
        "gz_dps",
        "mph",
        "event",
        "pitch_deg",
        "roll_deg",
        "x_east_m",
        "y_north_m",
        "z_m",
    ])
    log.flush()
    flush_count = 0

    buf = b""
    pts = []
    event_segments = []
    lat0 = None
    lon0 = None
    last_plot_update = 0.0

    app = QtWidgets.QApplication([])

    w = gl.GLViewWidget()
    w.setWindowTitle("Telemetry 3D Path")
    w.setGeometry(200, 100, 900, 700)
    w.show()

    status_window = QtWidgets.QWidget()
    status_window.setWindowTitle("System Status")
    status_window.setGeometry(1150, 100, 400, 250)

    status_layout = QtWidgets.QVBoxLayout()

    sats_label = QtWidgets.QLabel("SATS: --")
    nose_label = QtWidgets.QLabel("NOSE: --")
    radio_label = QtWidgets.QLabel("RADIO: --")
    temp_label = QtWidgets.QLabel("TEMP: --")

    for lbl in [sats_label, nose_label, radio_label, temp_label]:
        lbl.setAlignment(QtCore.Qt.AlignmentFlag.AlignCenter)
        lbl.setStyleSheet(
            "font-size: 22px; "
            "font-family: monospace; "
            "color: #ffff00; "
            "background-color: black;"
        )
        status_layout.addWidget(lbl)

    status_window.setLayout(status_layout)
    status_window.show()

    attitude_window = AttitudeWidget()
    attitude_window.setGeometry(1150, 380, 320, 320)
    attitude_window.show()

    grid_pos, grid_color = build_rect_grid(GRID_HALF_SIZE_M, GRID_STEP_M)
    ground_grid = gl.GLLinePlotItem(
        pos=grid_pos,
        color=grid_color,
        width=1,
        antialias=True,
        mode="lines",
    )
    w.addItem(ground_grid)

    east_axis = gl.GLLinePlotItem(
        pos=np.array([[0, 0, 0], [AX_LEN_M, 0, 0]], dtype=float),
        color=(1, 0, 0, 1),
        width=3,
        antialias=True,
    )
    north_axis = gl.GLLinePlotItem(
        pos=np.array([[0, 0, 0], [0, AX_LEN_M, 0]], dtype=float),
        color=(0, 1, 0, 1),
        width=3,
        antialias=True,
    )
    up_axis = gl.GLLinePlotItem(
        pos=np.array([[0, 0, 0], [0, 0, AX_LEN_M]], dtype=float),
        color=(0.2, 0.6, 1.0, 1),
        width=3,
        antialias=True,
    )
    w.addItem(east_axis)
    w.addItem(north_axis)
    w.addItem(up_axis)

    line = gl.GLLinePlotItem(
        pos=np.array([[0, 0, 0]], dtype=float),
        color=(1, 1, 0, 1),
        width=2,
        antialias=True,
    )
    start_marker = gl.GLScatterPlotItem(
        pos=np.array([[0, 0, 0]], dtype=float),
        size=12,
        color=(0, 1, 0, 1),
    )
    current_marker = gl.GLScatterPlotItem(
        pos=np.array([[0, 0, 0]], dtype=float),
        size=14,
        color=(1, 0, 0, 1),
    )
    w.addItem(line)
    w.addItem(start_marker)
    w.addItem(current_marker)

    event_lines = gl.GLLinePlotItem(
        pos=np.zeros((0, 3), dtype=float),
        color=(1, 0, 1, 1),
        width=2,
        antialias=True,
        mode="lines",
    )
    w.addItem(event_lines)

    def update():
        nonlocal buf, pts, event_segments, lat0, lon0, last_plot_update, flush_count

        chunk = ser.read(4096)
        if chunk:
            buf += chunk

        while b"\n" in buf:
            raw, buf = buf.split(b"\n", 1)
            s = raw.decode(errors="ignore").strip()

            if s.startswith("STATUS,"):
                try:
                    parts = s.split(",")

                    sats = int(parts[1].split("=")[1].strip())
                    nose = parts[2].split("=")[1].strip()
                    radio = parts[3].split("=")[1].strip()
                    temp = parts[4].split("=")[1].strip()

                    if sats < 4:
                        sats_color = "#ff0000"
                    elif sats == 4:
                        sats_color = "#ffff00"
                    else:
                        sats_color = "#00ff00"

                    nose_color = "#ff0000" if nose.upper() == "CRITICAL" else "#00ff00"

                    if radio.upper() == "LOST":
                        radio_color = "#ff0000"
                    elif radio.upper() == "DEGRADED":
                        radio_color = "#ffff00"
                    else:
                        radio_color = "#00ff00"

                    temp_color = "#00ff00"

                    set_status_label(sats_label, f"SATS: {sats}", sats_color)
                    set_status_label(nose_label, f"NOSE: {nose}", nose_color)
                    set_status_label(radio_label, f"RADIO: {radio}", radio_color)
                    set_status_label(temp_label, f"TEMP: {temp} C", temp_color)

                except Exception:
                    pass

                continue

            if not s.startswith("CSV,"):
                continue

            parts = s.split(",")
            if len(parts) < EXPECTED_FIELDS:
                continue

            try:
                seq = int(float(parts[IDX["seq"]]))
                t_ms = int(float(parts[IDX["t_ms"]]))
                lat = float(parts[IDX["lat"]])
                lon = float(parts[IDX["lon"]])
                alt_gnss = float(parts[IDX["alt_gnss_m"]])
                pres_hPa = float(parts[IDX["pres_hPa"]])
                alt_baro = float(parts[IDX["alt_baro_m"]])
                temp_C = float(parts[IDX["temp_C"]])
                sats = int(float(parts[IDX["sats"]]))
                ax_g = float(parts[IDX["ax_g"]])
                ay_g = float(parts[IDX["ay_g"]])
                az_g = float(parts[IDX["az_g"]])
                gx_dps = float(parts[IDX["gx_dps"]])
                gy_dps = float(parts[IDX["gy_dps"]])
                gz_dps = float(parts[IDX["gz_dps"]])
                mph = float(parts[IDX["mph"]])
                event = int(float(parts[IDX["event"]]))
            except (ValueError, IndexError):
                continue

            # ------------------------------------------------------------
            # ORIENTATION ALWAYS UPDATES, EVEN WITHOUT GPS LOCK
            # Y axis controls nose/pitch.
            # ------------------------------------------------------------
            pitch_deg = math.degrees(
                math.atan2(ay_g, math.sqrt(ax_g * ax_g + az_g * az_g))
            )

            roll_deg = math.degrees(
                math.atan2(ax_g, az_g)
            )

            attitude_window.set_attitude(roll_deg, pitch_deg)

            # If GPS is not locked, stop only the GPS/path plotting part.
            # IMU/orientation above still works.
            if sats < 4:
                writer.writerow([
                    seq, t_ms, lat, lon, alt_gnss, pres_hPa, alt_baro, temp_C, sats,
                    ax_g, ay_g, az_g, gx_dps, gy_dps, gz_dps, mph, event,
                    pitch_deg, roll_deg,
                    "", "", ""
                ])
                flush_count += 1
                if flush_count >= FLUSH_EVERY:
                    log.flush()
                    flush_count = 0
                continue

            if lat0 is None:
                lat0, lon0 = lat, lon
                start_marker.setData(pos=np.array([[0, 0, 0]], dtype=float))

            x, y = ll_to_local_xy(lat0, lon0, lat, lon)
            z = alt_baro if USE_BARO_ALT else alt_gnss

            writer.writerow([
                seq, t_ms, lat, lon, alt_gnss, pres_hPa, alt_baro, temp_C, sats,
                ax_g, ay_g, az_g, gx_dps, gy_dps, gz_dps, mph, event,
                pitch_deg, roll_deg,
                x, y, z
            ])
            flush_count += 1
            if flush_count >= FLUSH_EVERY:
                log.flush()
                flush_count = 0

            if event == 1:
                event_segments.append([[x, y, 0.0], [x, y, z]])
                seg_arr = np.array(event_segments, dtype=float).reshape(-1, 3)
                event_lines.setData(pos=seg_arr)

            pts.append([x, y, z])
            if len(pts) > MAX_POINTS:
                pts = pts[-MAX_POINTS:]

            now_plot = time.monotonic()
            if now_plot - last_plot_update >= (1.0 / PLOT_HZ):
                last_plot_update = now_plot

                arr = np.array(pts, dtype=float)
                line.setData(pos=arr)
                current_marker.setData(pos=np.array([[x, y, z]], dtype=float))

                if len(arr) > 1:
                    w.opts["center"] = pg.Vector(
                        float(np.mean(arr[:, 0])),
                        float(np.mean(arr[:, 1])),
                        float(np.mean(arr[:, 2])),
                    )
                    w.opts["distance"] = max(
                        80.0,
                        float(np.max(np.std(arr, axis=0)) * 6.0),
                    )

    timer = QtCore.QTimer()
    timer.timeout.connect(update)
    timer.start(30)

    try:
        app.exec()
    finally:
        try:
            log.flush()
        except Exception:
            pass
        try:
            log.close()
        except Exception:
            pass
        try:
            ser.close()
        except Exception:
            pass


if __name__ == "__main__":
    main()