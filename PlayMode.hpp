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

// Note types
enum NoteType : uint16_t {
	SINGLE,
	HOLD,
	BURST,
};

// Struct containing informations relevant to a single note
// TODO: Consider which informations is really necessary
// TODO: change the vectors to singular values
struct NoteInfo {
	std::vector<Scene::Transform *> note_transforms;
	std::vector<float> hit_times;
	NoteType noteType = NoteType::SINGLE;
	std::string dir;
	float coord_begin;
	float coord_end;

	glm::vec3 min = glm::vec3();
	glm::vec3 max = glm::vec3();

	glm::vec3 scale = glm::vec3();

	// We need both beenHit and isActive because otherwise notes that has been
	// hit will keep re-activating
	bool been_hit = false;
	bool is_active = false;
};

// struct for intersection functions
struct HitInfo {
	struct NoteInfo *note;
	float time;
};

struct PlayMode : Mode {
	// constructor
	PlayMode();
	// destructor
	virtual ~PlayMode();

	// functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	// initialization functions to make a notes vector from a beatmap
	void read_notes(std::string song_name);
 	glm::vec2 get_coords(std::string dir, float coord);
	

	// update functions - background and notes
	void update_bg(float elapsed);
	void update_notes();

	// intersection functions to hit notes
	bool bbox_intersect(glm::vec3 pos, glm::vec3 dir, glm::vec3 min, glm::vec3 max, float &t);
	HitInfo trace_ray(glm::vec3 pos, glm::vec3 dir);
	void check_hit();
	void hit_note(NoteInfo* note, int hit_status);

	void change_gun(int idx_change, int manual_idx);

	// game state related functions
	void reset_song();
	void to_menu();
	void start_song(int idx, bool restart);
	void restart_song();
	void pause_song();
	void unpause_song();
	void game_over(bool did_clear);

	//----- game state -----
	enum GameState {
		PLAYING,
		PAUSED,
		MENU,
		SONGCLEAR,
		GAMEOVER,
	} game_state;

	// variable to keep track if mouse click is being held down
	bool holding = false;

	// local copy of the game scene
	Scene scene;

	// pointer to camera to manipulate
	Scene::Camera *camera = nullptr;

	// background scrolling
	std::vector<Scene::Transform *> bg_transforms;

	// assets
	MeshBuffer const *meshBuf;

	// vector containing all note infos of a song
	std::vector<NoteInfo> notes;
	
	// vector containing a name and a vector of skins
	std::vector< std::pair<std::string, std::vector<Drawable>> > beatmap_skins;
	int active_skin_idx = 0;

	// vector containing list of songs
	std::vector< std::pair<std::string, Sound::Sample> > song_list;

	// health bar
	Drawable healthbar_drawable;
	Scene::Transform *healthbar_transform = nullptr;
	glm::vec3 const healthbar_position = glm::vec3(-0.75f, 0.75f, -4.8f); // TODO: change this
	glm::vec3 const healthbar_scale = glm::vec3(0.5f, 0.5f, 0.5f);
	Drawable health_drawable;
	Scene::Transform *health_transform = nullptr;

	// gun information
	Drawable gun_drawable;
	std::vector<Scene::Transform *> gun_transforms;
	glm::vec3 const gun_scale = glm::vec3(0.02f, 0.02f, 0.05f);
	int gun_mode = 0; // 0 = single, 1 = burst, 2 = hold

	// border information
	Drawable border_drawable;
	Scene::Transform *border_transform = nullptr;
	float x_scale = 1.0f;
	float y_scale = 1.0f;

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
	float const max_health = 1.0f;

	// UI
	std::vector<std::string> option_texts {"RESUME", "RESTART", "EXIT"};
	std::vector<std::string> songover_texts {"RESTART", "EXIT"};
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

		//update camera aspect ratio for drawable:
		camera->transform->rotation =
			normalize(glm::angleAxis(cam.azimuth, glm::vec3(0.0f, 1.0f, 0.0f))
			* glm::angleAxis(0.5f * 3.1415926f + -cam.elevation, glm::vec3(1.0f, 0.0f, 0.0f)))
		;
	}
};
