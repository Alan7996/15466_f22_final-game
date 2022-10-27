#include "Mode.hpp"

#include "Scene.hpp"
#include "WalkMesh.hpp"

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
	Scene::Transform *note_transform;
	float hitTime = 0.0f;
	NoteType noteType = NoteType::SINGLE;
};

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	// functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	// read from notes.txt
	// format: up/down/left/right, coord, hit time, and note type (optional, default to single) separated by space
	// if up/down, -y_scale <= coord <= y_scale
	// if left/right, -x_scale <= coord <= x_scale
	virtual void read_notes();

	// update note visibility and position
	virtual void update_notes();

	// cast a ray to detect collision with a mesh
	// virtual bool trace_ray();
	// // see if you hit the notes function
	// virtual bool hit_notes();

	//----- game state -----

	// input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up;

	// local copy of the game scene
	Scene scene;

	Scene::Camera *camera = nullptr;

	// assets
	Drawable note_drawable;
	std::vector<NoteInfo> notes;

	Drawable gun_drawable;
	Scene::Transform *gun_transform = nullptr;

	Drawable border_drawable;
	Scene::Transform *border_transform = nullptr;
	float x_scale = 1.0f;
	float y_scale = 1.0f;

	// music
	bool has_started = false;
	std::chrono::time_point<std::chrono::high_resolution_clock> music_start_time;

	// settings
	float init_note_depth = -20.0f;
	float border_depth = -0.0f;
	float note_approach_time = 4.0f; // time between when the note shows up and hit time
	float valid_hit_time_delta = 0.2f;

	// from ShowSceneMode.hpp
	struct {
		float radius = 2.0f;
		float azimuth = 0.3f; //angle ccw of -y axis, in radians, [-pi,pi]
		float elevation = 0.2f; //angle above ground, in radians, [-pi,pi]
		glm::vec3 target = glm::vec3(0.0f);
		bool flip_x = false; //flip x inputs when moving? (used to handle situations where camera is upside-down)
	} cam;

};
