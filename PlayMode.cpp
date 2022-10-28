#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>

#include <random>
#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include "glm/gtx/string_cast.hpp"

GLuint main_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > main_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("main.pnct"));
	main_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > main_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("main.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = main_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = main_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;
	});
});

PlayMode::PlayMode() : scene(*main_scene) {
	// camera and assets
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();
	for (auto &d : scene.drawables) {
		if (d.transform->name == "Note") {
			note_drawable.type = d.pipeline.type;
			note_drawable.start = d.pipeline.start;
			note_drawable.count = d.pipeline.count;
		} else if (d.transform->name == "Gun") {
			gun_drawable.type = d.pipeline.type;
			gun_drawable.start = d.pipeline.start;
			gun_drawable.count = d.pipeline.count;
		} else if (d.transform->name == "Border") {
			border_drawable.type = d.pipeline.type;
			border_drawable.start = d.pipeline.start;
			border_drawable.count = d.pipeline.count;
		} 
	}
	scene.drawables.clear();

	{ // initialize game state
		// gun_transform = new Scene::Transform;
		// gun_transform->name = "Gun";
		// gun_transform->position = glm::vec3(0.0f, 0.0f, 0.0f);
		// gun_transform->scale = glm::vec3(1.0f, 1.0f, 1.0f);
		// scene.drawables.emplace_back(gun_transform);
		// Scene::Drawable &d1 = scene.drawables.back();
		// d1.pipeline = lit_color_texture_program_pipeline;
		// d1.pipeline.vao = main_meshes_for_lit_color_texture_program;
		// d1.pipeline.type = gun_drawable.type;
		// d1.pipeline.start = gun_drawable.start;
		// d1.pipeline.count = gun_drawable.count;

		border_transform = new Scene::Transform;
		border_transform->name = "Border";
		border_transform->position = glm::vec3(0.0f, 0.0f, border_depth);
		border_transform->scale = glm::vec3(x_scale, y_scale, 0.01f);
		scene.drawables.emplace_back(border_transform);
		Scene::Drawable &d2 = scene.drawables.back();
		d2.pipeline = lit_color_texture_program_pipeline;
		d2.pipeline.vao = main_meshes_for_lit_color_texture_program;
		d2.pipeline.type = border_drawable.type;
		d2.pipeline.start = border_drawable.start;
		d2.pipeline.count = border_drawable.count;

		read_notes();
	}
}

PlayMode::~PlayMode() {
}

// https://java2blog.com/split-string-space-cpp/
void tokenize(std::string const &str, const char* delim, std::vector<std::string> &out) {
	char *next_token, *token;
	#ifdef _WIN32
    	token = strtok_s(const_cast<char*>(str.c_str()), delim, &next_token);
	#else
		token = strtok_r(const_cast<char*>(str.c_str()), delim, &next_token);
	#endif
    while (token != nullptr) {
        out.push_back(std::string(token));
		#ifdef _WIN32
			token = strtok_s(nullptr, delim, &next_token);
		#else
			token = strtok_r(nullptr, delim, &next_token);
		#endif
        
    }
}

std::pair<float, float> PlayMode::get_coords(std::string dir, float coord) {
	float x = 0.0f;
	float y = 0.0f;
	if (dir == "left") {
		x = coord;
		y = y_scale;
	} else if (dir == "right") {
		x = coord;
		y = -y_scale;
	} else if (dir == "up") {
		x = x_scale;
		y = coord;
	} else if (dir == "down") {
		x = -x_scale;
		y = coord;
	} 
	return std::make_pair(x, y);
}

void PlayMode::read_notes() {
	// https://www.tutorialspoint.com/read-file-line-by-line-using-cplusplus
	std::fstream file;
	const char* delim = " ";
	file.open(data_path("notes.txt"), std::ios::in);
	if (file.is_open()){
		std::string line;
		while(getline(file, line)){
			std::vector<std::string> note_info;
			tokenize(line, delim, note_info);
			std::string note_type = note_info[0];
			std::string dir = note_info[1];
			int idx = (int) (find(note_info.begin(), note_info.end(), "@") - note_info.begin());
			if (note_type == "hold") {
				NoteInfo note;
				note.noteType = NoteType::HOLD;
				for (int i = 0; i < idx - 2; i++) {
					float coord = std::stof(note_info[2+i]);
					float time = std::stof(note_info[idx+1+i]);
					std::pair<float, float> coords = get_coords(dir, coord);

					Scene::Transform *transform = new Scene::Transform;
					transform->name = "Note";
					transform->position = glm::vec3(coords.first, coords.second, init_note_depth);
					transform->scale = glm::vec3(0.0f, 0.0f, 0.0f); // all notes start from being invisible
					note.note_transforms.push_back(transform);
					note.hit_times.push_back(time);
				}
				notes.push_back(note);

				for (uint64_t i = 0; i < note.note_transforms.size(); i++) {
					scene.drawables.emplace_back(note.note_transforms[i]);
					Scene::Drawable &d = scene.drawables.back();
					d.pipeline = lit_color_texture_program_pipeline;
					d.pipeline.vao = main_meshes_for_lit_color_texture_program;
					d.pipeline.type = note_drawable.type;
					d.pipeline.start = note_drawable.start;
					d.pipeline.count = note_drawable.count;
				}
			} else {
				float coord = std::stof(note_info[2]);
				float time = std::stof(note_info[4]);
				NoteType type = NoteType::SINGLE;
				if (note_type == "burst") type = NoteType::BURST;
				std::pair<float, float> coords = get_coords(dir, coord);
				
				NoteInfo note;
				note.noteType = type;
				Scene::Transform *transform = new Scene::Transform;
				transform->name = "Note";
				transform->position = glm::vec3(coords.first, coords.second, init_note_depth);
				transform->scale = glm::vec3(0.0f, 0.0f, 0.0f); // all notes start from being invisible
				note.note_transforms.push_back(transform);
				note.hit_times.push_back(time);
				notes.push_back(note);

				scene.drawables.emplace_back(note.note_transforms[0]);
				Scene::Drawable &d = scene.drawables.back();
				d.pipeline = lit_color_texture_program_pipeline;
				d.pipeline.vao = main_meshes_for_lit_color_texture_program;
				d.pipeline.type = note_drawable.type;
				d.pipeline.start = note_drawable.start;
				d.pipeline.count = note_drawable.count;
			}
		}
		file.close();
	}
}

void PlayMode::update_notes() {
	if (!has_started) {
		return;
	}
	auto current_time = std::chrono::high_resolution_clock::now();
	float music_time = std::chrono::duration<float>(current_time - music_start_time).count();

	for (auto &note : notes) {
		for (uint64_t i = 0; i < note.note_transforms.size(); i++) {
			if (music_time < note.hit_times[i] - note_approach_time) {
				continue; // not yet time to show this note
			} else if (music_time > note.hit_times[i] + valid_hit_time_delta) {
				note.note_transforms[i]->scale = glm::vec3(0.0f, 0.0f, 0.0f); // note already gone
				// TODO: animation
			} else {
				// move note toward player
				note.note_transforms[i]->scale = glm::vec3(0.1f, 0.1f, 0.1f);
				float delta_time = music_time - (note.hit_times[i] - note_approach_time);
				float note_speed = (border_depth - init_note_depth) / note_approach_time;
				note.note_transforms[i]->position.z = init_note_depth + note_speed * delta_time;
			}	
		}
	}
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_KEYDOWN) {
		if (evt.key.keysym.sym == SDLK_RETURN) {
			// start game
			if (!has_started) {
				has_started = true;
				music_start_time = std::chrono::high_resolution_clock::now();
			}
			return true;
		}
		// TODO: hit note
	}
	return false;
}

void PlayMode::update(float elapsed) {
	//reset button press counters:
	left.downs = 0;
	right.downs = 0;
	up.downs = 0;
	down.downs = 0;

	update_notes();
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	glUseProgram(0);

	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	scene.draw(*camera);

	/* In case you are wondering if your walkmesh is lining up with your scene, try:
	{
		glDisable(GL_DEPTH_TEST);
		DrawLines lines(player.camera->make_projection() * glm::mat4(player.camera->transform->make_world_to_local()));
		for (auto const &tri : walkmesh->triangles) {
			lines.draw(walkmesh->vertices[tri.x], walkmesh->vertices[tri.y], glm::u8vec4(0x88, 0x00, 0xff, 0xff));
			lines.draw(walkmesh->vertices[tri.y], walkmesh->vertices[tri.z], glm::u8vec4(0x88, 0x00, 0xff, 0xff));
			lines.draw(walkmesh->vertices[tri.z], walkmesh->vertices[tri.x], glm::u8vec4(0x88, 0x00, 0xff, 0xff));
		}
	}
	*/

	{ //use DrawLines to overlay some text:
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));

		constexpr float H = 0.09f;
		lines.draw_text("Mouse motion looks; WASD moves; escape ungrabs mouse",
			glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		float ofs = 2.0f / drawable_size.y;
		lines.draw_text("Mouse motion looks; WASD moves; escape ungrabs mouse",
			glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + + 0.1f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
	}
	GL_ERRORS();
}
