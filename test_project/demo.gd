extends Control
## Shows CPU, RAM and a seekable timeline for the song playing on $AudioStreamPlayer.
## Assign a VGMStream to the player's Stream property in the inspector, then run this scene,
## or drop a song file onto the window. Only get_cpu_usage() and get_memory_usage() come from the
## extension; the timeline, RAM/CPU readouts and drag and drop are this demo's own code.

@onready var player: AudioStreamPlayer = $AudioStreamPlayer
@onready var title_label: Label = %Title
@onready var cpu_label: Label = %CpuLabel
@onready var cpu_bar: ProgressBar = %CpuBar
@onready var ram_label: Label = %RamLabel
@onready var timeline: HSlider = %Timeline
@onready var time_label: Label = %TimeLabel
@onready var drop_label: Label = %DropLabel

const SONG_EXTENSIONS := ["vgm", "vgz", "s98", "dro", "gym"]
const DROP_HINT := "Drop a song file here (.vgz .vgm .s98 .dro .gym)"

var dragging := false
var process_cpu := -1.0 # Share of one core; -1 where unsupported.
var cpu_sample_ticks := 0
var cpu_sample_msec := 0


func _ready() -> void:
	timeline.drag_started.connect(func(): dragging = true)
	timeline.drag_ended.connect(_on_drag_ended)
	get_window().files_dropped.connect(_on_files_dropped)
	drop_label.text = DROP_HINT


func _process(_delta: float) -> void:
	var song := player.stream as VGMStream
	if song == null:
		title_label.text = "Assign a VGMStream to AudioStreamPlayer"
		return
	title_label.text = song.get_song_tags().get("TITLE", "(untitled)")

	var playback := player.get_stream_playback() as VGMStreamPlayback
	var cpu := playback.get_cpu_usage() if playback else 0.0
	var total := _process_cpu()
	if total < 0.0:
		cpu_label.text = "CPU  %.1f%% (extension)" % (cpu * 100.0)
	else:
		cpu_label.text = "CPU  %.1f%% (process)\n  Godot  %.1f%%\n  Extension  %.1f%%" % [
			total * 100.0,
			maxf(total - cpu, 0.0) * 100.0,
			cpu * 100.0,
		]
	cpu_bar.value = cpu

	var song_ram := playback.get_memory_usage() if playback else 0
	var total_ram := _process_ram()
	ram_label.text = "RAM  %s\n  Godot  %.1f MB\n  Song  %s" % [
		"%.1f MB (process)" % (total_ram / 1048576.0) if total_ram >= 0 else "",
		OS.get_static_memory_usage() / 1048576.0,
		"%.2f MB" % (song_ram / 1048576.0) if song_ram > 0 else "n/a",
	]

	timeline.max_value = song.get_length()
	if not dragging:
		timeline.value = player.get_playback_position()
	time_label.text = "%s / %s" % [_format_time(timeline.value), _format_time(timeline.max_value)]


## Plays the first dropped file that loads as a song. The whole window accepts drops.
func _on_files_dropped(files: PackedStringArray) -> void:
	for path in files:
		if path.get_extension().to_lower() not in SONG_EXTENSIONS:
			continue
		var song := load(path) as VGMStream
		if song:
			player.stream = song
			player.play()
			drop_label.text = "Playing " + path.get_file()
			return
	drop_label.text = "Not a song file. " + DROP_HINT


func _on_drag_ended(_value_changed: bool) -> void:
	dragging = false
	player.seek(timeline.value)


## Share of one core the whole process used, over windows of at least half a second.
func _process_cpu() -> float:
	var now := Time.get_ticks_msec()
	if now - cpu_sample_msec < 500:
		return process_cpu
	var ticks := _process_cpu_ticks()
	if ticks < 0:
		return -1.0
	if cpu_sample_msec > 0:
		process_cpu = (ticks - cpu_sample_ticks) / 100.0 / ((now - cpu_sample_msec) / 1000.0)
	cpu_sample_ticks = ticks
	cpu_sample_msec = now
	return process_cpu


## User + system CPU ticks (1/100 s) of every thread, from /proc/self/stat. -1 outside Linux.
func _process_cpu_ticks() -> int:
	var stat := FileAccess.open("/proc/self/stat", FileAccess.READ)
	if stat == null:
		return -1
	var line := stat.get_line()
	var fields := line.substr(line.rfind(")") + 2).split(" ") # Skips "pid (name)"; fields[0] is the state.
	return fields[11].to_int() + fields[12].to_int()


## The libvgm emulators allocate outside Godot's memory tracking, so use the whole process's resident memory. -1 where unsupported.
func _process_ram() -> int:
	var status := FileAccess.open("/proc/self/status", FileAccess.READ)
	if status == null:
		return -1 # No /proc outside Linux and Android.
	while not status.eof_reached():
		var line := status.get_line()
		if line.begins_with("VmRSS:"):
			return line.substr(6).strip_edges().to_int() * 1024 # Reported in kB.
	return 0


func _format_time(seconds: float) -> String:
	return "%d:%02d" % [int(seconds) / 60, int(seconds) % 60]
