from flask import Flask, render_template, send_from_directory

app = Flask(__name__)


@app.route("/")
def index():
    return render_template("index.html")


@app.route("/video/<path:filename>")
def video(filename):
    return send_from_directory("videos", filename)


@app.route("/hls/<path:filename>")
def hls(filename):
    return send_from_directory("hls", filename)


@app.route("/hls")
def hls_index():
    return render_template("hls.html")


@app.route("/dash/<path:filename>")
def dash(filename):
    return send_from_directory("dash", filename)


@app.route("/dash")
def dash_index():
    return render_template("dash.html")


if __name__ == "__main__":
    app.run(host="0.0.0.0", debug=True)


# DASH video 만드는 방법
# ffmpeg -i video02.mp4 -c:v libx264 -b:v 1M -c:a aac -b:a 128k -vf "scale=-1:720" -f dash -min_seg_duration 5000 video02.mpd

# HLS 만드는 방법
# ffmpeg -i video02.mp4 -profile:v baseline -level 3.0 -s 640x480 -start_number 0 -hls_time 10 -hls_list_size 0 -f hls index.m3u8
