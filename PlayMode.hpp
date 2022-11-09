#include "Mode.hpp"

#include "Scene.hpp"
#include "WalkMesh.hpp"
#include "Sound.hpp"
#include "Mesh.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>
#include <chrono>

struct Drawable {
	GLenum type = GL_TRIANGLES;
	GLuint start = 0;
	GLuint count = 0; 
};

enum NoteType : uint16_t { // TODO
	SINGLE,
	HOLD,
	BURST,
};

struct NoteInfo {
	std::vector<Scene::Transform *> note_transforms;
	std::vector<float> hit_times;
	NoteType noteType = NoteType::SINGLE;
	std::string dir;
	float coord_begin;
	float coord_end;

	glm::vec3 min = glm::vec3();
	glm::vec3 max = glm::vec3();

	// We need both beenHit and isActive because otherwise notes that has been
	// hit will keep re-activating
	bool beenHit = false;
	bool isActive = false;
};

struct HitInfo {
	// might need smthing like isHitting for hold
	struct NoteInfo *note;
	float time;
};

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	// functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	// read from notes.txt
	// format: [space separated] single/burst/hold, up/down/left/right, coord(s), @, hit time(s)
	// if up/down, -y_scale <= coord <= y_scale
	// if left/right, -x_scale <= coord <= x_scale
	virtual void read_notes(std::string song_name);
	virtual glm::vec2 get_coords(std::string dir, float coord);

	// update note visibility and position
	void update_bg(float elapsed);
	virtual void update_notes();
	void hit_note(NoteInfo* note, int hit_status);

	// cast a ray to detect collision with a mesh
	bool bbox_intersect(glm::vec3 pos, glm::vec3 dir, glm::vec3 min, glm::vec3 max, float &t);
	HitInfo trace_ray(glm::vec3 pos, glm::vec3 dir);
	void check_hit();

	// read the .wav file
	void read_song();

	// game state related
	void reset_song();
	void to_menu();
	void start_song(int idx, bool restart);
	void restart_song();
	void pause_song();
	void unpause_song();
	void game_over(bool didClear);

	//----- game state -----
	enum GameState {
		PLAYING,
		PAUSED,
		MENU,
		GAMEOVER,
	} gameState;

	// input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} hold;
	//variable to keep track if mouse click is being held down
	bool holding = false;

	// local copy of the game scene
	Scene scene;

	Scene::Camera *camera = nullptr;

	// background scrolling
	std::vector<Scene::Transform *> bg_transforms;

	// assets
	// TODO : edit so that gun and border_drawable's are not drawn in menu
	MeshBuffer const *meshBuf;

	std::vector<NoteInfo> notes;
	
	std::vector< std::pair<std::string, std::vector<Drawable>> > beatmap_skins;
	int active_skin_idx = 0;

	Drawable gun_drawable;
	Scene::Transform *gun_transform = nullptr;

	Drawable border_drawable;
	Scene::Transform *border_transform = nullptr;
	float x_scale = 1.0f;
	float y_scale = 1.0f;

	std::vector< std::pair<std::string, Sound::Sample> > song_list;

	// music & SFX
	bool has_started = false;
	std::chrono::time_point<std::chrono::high_resolution_clock> music_start_time;
	std::chrono::time_point<std::chrono::high_resolution_clock> music_pause_time;
	std::shared_ptr< Sound::PlayingSample > active_song;

	Sound::Sample note_hit_sound;
	Sound::Sample note_miss_sound;

	// gameplay
	int note_start_idx = 0;
	int note_end_idx = 0;
	int score = 0;
	int combo = 0;
	int multiplier = 1;
	float health = 0.7f;
	float const maxHealth = 1.0f;

	// UI
	std::vector<std::string> option_texts {"RESUME", "RESTART", "EXIT"};
	uint8_t hovering_text = 0;
	int chosen_song = 0;

	// settings
	// TODO : should include some scaling variable to allow for different note speed settings to automatically affect these
	float init_note_depth = -20.0f;
	float border_depth = -0.0f;
	float max_depth = 10.0f;
	float note_approach_time = 4.0f; // time between when the note shows up and hit time
	float valid_hit_time_delta = 0.3f;
	float real_song_offset = 0.075f;

	float bgscale;

	float mouse_sens = 0.4f;
	float const mouse_sens_min = 0.1f;
	float const mouse_sens_max = 1.0f;
	float const mouse_sens_inc = 0.1f;


	// from ShowSceneMode.hpp to fix the up axis
	struct {
		float radius = 2.0f;
		float azimuth = 0.0f; //angle ccw of -y axis, in radians, [-pi,pi]
		float elevation = 3.1415926f / 2.0f; //angle above ground, in radians, [-pi,pi]
		glm::vec3 target = glm::vec3(0.0f);
		bool flip_x = false; //flip x inputs when moving? (used to handle situations where camera is upside-down)
	} cam;

	void reset_cam() { 
		cam.radius = 2.0f;
		cam.azimuth = 0.0f;
		cam.elevation = 3.1415926f / 2.0f;
		cam.target = glm::vec3(0.0f);
		cam.flip_x = false;
	}
};
