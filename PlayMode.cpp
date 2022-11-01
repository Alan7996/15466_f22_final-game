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
#include <array>
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

// would like to generalize this load_song function to take in string input and load string.wav file
Load< Sound::Sample > load_song_tutorial(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("Tutorial.wav"));
});

PlayMode::PlayMode() : scene(*main_scene) {
	SDL_SetRelativeMouseMode(SDL_TRUE);

	meshBuf = new MeshBuffer(data_path("main.pnct"));

	// camera and assets
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();

	std::vector<Drawable> default_skin(15);
	for (auto &d : scene.drawables) {
		if (d.transform->name.find("Note") != std::string::npos) {
			// set up default skin array
			// change in future to support names >= 10
			int idx = d.transform->name.at(4) - '0';
			default_skin[idx].type = d.pipeline.type;
			default_skin[idx].start = d.pipeline.start;
			default_skin[idx].count = d.pipeline.count;
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

	beatmap_skins.emplace_back(std::make_pair("Note", default_skin));
	scene.drawables.clear();

	{ // initialize game state
		gun_transform = new Scene::Transform;
		gun_transform->name = "Gun";
		gun_transform->parent = camera->transform;
		// TODO: these numbers need tweaking once we finalize the gun model
		gun_transform->position = glm::vec3(0.03f, -0.06f, -0.4f);
		gun_transform->scale = glm::vec3(0.01f, 0.01f, 0.1f);
		gun_transform->rotation = glm::quat(0.0f, 0.0f, 1.0f, 0.0f);
		scene.drawables.emplace_back(gun_transform);
		Scene::Drawable &d1 = scene.drawables.back();
		d1.pipeline = lit_color_texture_program_pipeline;
		d1.pipeline.vao = main_meshes_for_lit_color_texture_program;
		d1.pipeline.type = gun_drawable.type;
		d1.pipeline.start = gun_drawable.start;
		d1.pipeline.count = gun_drawable.count;

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

		// would be nice to count the number of songs / know their names by reading through the file system
		// all tutorial songs for testing purposes for now
		song_list.emplace_back(std::make_pair("Tutorial", *load_song_tutorial));
		song_list.emplace_back(std::make_pair("Tutorial2", *load_song_tutorial));
		song_list.emplace_back(std::make_pair("Tutorial3", *load_song_tutorial));
		song_list.emplace_back(std::make_pair("Tutorial4", *load_song_tutorial));
		song_list.emplace_back(std::make_pair("Tutorial5", *load_song_tutorial));
		song_list.emplace_back(std::make_pair("Tutorial6", *load_song_tutorial));
		song_list.emplace_back(std::make_pair("Tutorial7", *load_song_tutorial));

		to_menu();
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

void PlayMode::read_notes(std::string song_name) {
	notes.clear();

	// https://www.tutorialspoint.com/read-file-line-by-line-using-cplusplus
	std::fstream file;
	const char* delim = " ";
	file.open(data_path(song_name + ".txt"), std::ios::in);
	if (file.is_open()){
		std::string line;
		while(getline(file, line)){
			std::vector<std::string> note_info;
			tokenize(line, delim, note_info);
			std::string note_type = note_info[0];
			int note_mesh_idx = stoi(note_info[1]);
			std::string dir = note_info[2];
			int idx = (int) (find(note_info.begin(), note_info.end(), "@") - note_info.begin());

			NoteInfo note;
			Mesh note_mesh = meshBuf->lookup(beatmap_skins[active_skin_idx].first + note_info[1]);
			note.min = note_mesh.min;
			note.max = note_mesh.max;
			// this requires a bit more thinking on how to handle hold notes

			if (note_type == "hold") {
				note.noteType = NoteType::HOLD;

				for (int i = 0; i < idx - 4; i++) {
					float coord_begin = std::stof(note_info[3+i]);
					float time_begin = std::stof(note_info[idx+1+i]);

					float coord_end = std::stof(note_info[3+i+1]);
					float time_end = std::stof(note_info[idx+1+i+1]);
					std::pair<float, float> coords_begin = get_coords(dir, coord_begin);
					std::pair<float, float> coords_end = get_coords(dir, coord_end);

					Scene::Transform *transform = new Scene::Transform;
					transform->name = "Note";
					transform->position = glm::vec3((coords_begin.first + coords_end.first) / 2.0f, (coords_begin.second + coords_end.second) / 2.0f, init_note_depth);
					transform->scale = glm::vec3(0.0f, 0.0f, 0.0f); // all notes start from being invisible
					float angle = 0.0f;
					// if the xs are the same
					if(coords_begin.first == coords_end.first) {
						angle = atan2(fabsf(coords_begin.second - coords_end.second) / 2.0f, time_end - time_begin);
					}
					// otherwise the ys are the same
					else {
						angle = atan2(fabsf(coords_begin.first - coords_end.first) / 2.0f, time_end - time_begin);
					}
					transform->rotation = normalize(glm::angleAxis(angle, glm::vec3(1.0f, 0.0f, 0.0f)));

					note.note_transforms.push_back(transform);
					note.hit_times.push_back(time_begin);
					note.hit_times.push_back(time_end);
				}
			} else {
				float coord = std::stof(note_info[3]);
				float time = std::stof(note_info[5]);
				std::pair<float, float> coords = get_coords(dir, coord);
				
				note.noteType = note_type == "single" ? NoteType::SINGLE : NoteType::BURST;

				Scene::Transform *transform = new Scene::Transform;
				transform->name = "Note";
				transform->position = glm::vec3(coords.first, coords.second, init_note_depth);
				transform->scale = glm::vec3(0.0f, 0.0f, 0.0f); // all notes start from being invisible

				note.note_transforms.push_back(transform);
				note.hit_times.push_back(time);
			}

			notes.push_back(note);

			for (int i = 0; i < (int)note.note_transforms.size(); i++) {
				scene.drawables.emplace_back(note.note_transforms[i]);
				Scene::Drawable &d = scene.drawables.back();
				d.pipeline = lit_color_texture_program_pipeline;
				d.pipeline.vao = main_meshes_for_lit_color_texture_program;
				d.pipeline.type = beatmap_skins[active_skin_idx].second[note_mesh_idx].type;
				d.pipeline.start = beatmap_skins[active_skin_idx].second[note_mesh_idx].start;
				d.pipeline.count = beatmap_skins[active_skin_idx].second[note_mesh_idx].count;
			}
		}
		file.close();
	}
}


/* We maintain the list of notes to be checked in the following way : 
 * We keep track of two types of variables. First are note_start_idx and 
 * note_end_idx, which records the range of notes that have spawned but have
 * yet to reach the disappearing line. Second are each note's isActive boolean,
 * which if a note was correctly hit by the player should toggle to false and
 * make the note have 0 scale. We however do not immediately update the indices,
 * meaning the note will continue to move towards the player until it reaches the
 * disappearing line. This is so that we can keep a nice loop from start to end
 * indices without worrying about tight time gaps between notes. 
 * 
 * Also note that we always loop up to ONE INDEX HIGHER than the end index, to
 * check if we should start the next note or not.
*/
void PlayMode::update_notes() {
	if (gameState != PLAYING) return;

	auto current_time = std::chrono::high_resolution_clock::now();
	float music_time = std::chrono::duration<float>(current_time - music_start_time).count();
	
	for (int i = note_start_idx; i < note_end_idx + 1; i++) {
		if (i >= (int)notes.size()) continue;
		auto &note = notes[i];
		for (int j = 0; j < (int)note.note_transforms.size(); j++) {
			if (note.isActive) {
				if (music_time > note.hit_times[j] + valid_hit_time_delta + real_song_offset) {
					// 'delete' the note
					note.note_transforms[j]->scale = glm::vec3(0.0f, 0.0f, 0.0f);
					note_start_idx += 1;

					if (note_start_idx == (int)notes.size()) game_over(true);
				}
				else {
					// move the note
					float delta_time = music_time - (note.hit_times[j] - note_approach_time);
					float note_speed = (border_depth - init_note_depth) / note_approach_time;
					note.note_transforms[j]->position.z = init_note_depth + note_speed * delta_time;
				}
			} else {
				if (!note.beenHit) {
					if (music_time >= note.hit_times[j] - note_approach_time + real_song_offset) {
						// spawn the note
						note.isActive = true;
						if(note.hit_times.size() > 1) {
							note.note_transforms[j]->scale = glm::vec3(0.1f, 0.1f, note.hit_times[1] - note.hit_times[0]);
							note.note_transforms[j]->position.z = init_note_depth;
						}
						else {
							note.note_transforms[j]->scale = glm::vec3(0.1f, 0.1f, 0.1f);
						}
						note_end_idx += 1;
					} else {
						continue;
					}
				}
			}
		}
	}
}

void PlayMode::hit_note(NoteInfo* note) {
	// deactivate the note
	note->beenHit = true;
	note->isActive = false;

	// TODO : fix this for hold
	note->note_transforms[0]->scale = glm::vec3(0.0f, 0.0f, 0.0f);

	// increment score & health

}

// bbox hit from https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-box-intersection
bool PlayMode::bbox_intersect(glm::vec3 pos, glm::vec3 dir, glm::vec3 min, glm::vec3 max) 
{ 
    float tmin = (min.x - pos.x) / dir.x; 
    float tmax = (max.x - pos.x) / dir.x; 
 
    if (tmin > tmax) std::swap(tmin, tmax); 
 
    float tymin = (min.y - pos.y) / dir.y; 
    float tymax = (max.y - pos.y) / dir.y; 
 
    if (tymin > tymax) std::swap(tymin, tymax); 
 
    if ((tmin > tymax) || (tymin > tmax)) 
        return false; 
 
    if (tymin > tmin) 
        tmin = tymin; 
 
    if (tymax < tmax) 
        tmax = tymax; 
 
    float tzmin = (min.z - pos.z) / dir.z; 
    float tzmax = (max.z - pos.z) / dir.z; 
 
    if (tzmin > tzmax) std::swap(tzmin, tzmax); 
 
    if ((tmin > tzmax) || (tzmin > tmax)) 
        return false; 
 
    if (tzmin > tmin) 
        tmin = tzmin; 
 
    if (tzmax < tmax) 
        tmax = tzmax; 
 
    return true; 
} 

// attempt bounding box implementation
HitInfo PlayMode::trace_ray(glm::vec3 pos, glm::vec3 dir) {
	for (int i = note_start_idx; i < note_end_idx; i++) {
		NoteInfo &note = notes[i];
		// single type
		if (note.noteType == NoteType::SINGLE) {
			// get transform
			Scene::Transform *trans = note.note_transforms[0];
			// transform bounding box of note to world space
			glm::vec3 trans_min = trans->make_local_to_parent() * glm::vec4(note.min, 1.f);
			glm::vec3 trans_max = trans->make_local_to_parent() * glm::vec4(note.max, 1.f);
			// do bbox intersection
			if(bbox_intersect(pos, dir, trans_min, trans_max)) {
				std::cout << "single\n";
				HitInfo hits;
				hits.note = &notes[i];
				return hits;
			}
		}
		// burst type
		// currently we have one model containing all three notes
		// this means we can shoot anything in between the three notes and it would count as a hit
		// will need to change to adding two additional drawables for vertical slice
		else if(note.noteType == NoteType::BURST) {
			// get transform
			Scene::Transform *trans = note.note_transforms[0];
			// transform bounding box of note to world space
			glm::vec3 trans_min = trans->make_local_to_parent() * glm::vec4(note.min, 1.f);
			glm::vec3 trans_max = trans->make_local_to_parent() * glm::vec4(note.max, 1.f);
			// do bbox intersection
			if(bbox_intersect(pos, dir, trans_min, trans_max)) {
				std::cout << "burst\n";
				HitInfo hits;
				hits.note = &notes[i];
				return hits;
			}
		}
		// hold type
		else if(note.noteType == NoteType::HOLD) {
			
		}
		else {
			std::cout << "Incorrect note type breaks the game." << "\n";
			exit(1);
		}
	}

	return HitInfo();
}

void PlayMode::check_hit() {
	// ray from camera position to origin (p1 - p2)
	glm::vec3 ray = glm::vec3(0) - camera->transform->position;
	// rotate ray to get the direction from camera
	ray = glm::rotate(camera->transform->rotation, ray);
	// trace the ray to see if we hit a note
	HitInfo hits = trace_ray(camera->transform->position, ray);

	auto current_time = std::chrono::high_resolution_clock::now();
	float music_time = std::chrono::duration<float>(current_time - music_start_time).count();
	// if we hit a note, check to see if we hit a good time
	if(hits.note) {
		// valid hit time
		if(fabs(music_time - hits.note->hit_times[0] + real_song_offset) < valid_hit_time_delta) {
			std::cout << "valid hit\n";
			hit_note(hits.note);
		}
		else {
			std::cout << "bad hit\n";
		}
	}
	else {
		std::cout << "miss\n";
	}
}

void PlayMode::reset_song() {
	// reset loaded assets
	if (active_song) active_song->stop();
	for (auto &note: notes) {
		note.beenHit = false;
		note.isActive = false;
		for (uint64_t i = 0; i < note.note_transforms.size(); i++) {
			note.note_transforms[i]->position = glm::vec3(note.note_transforms[i]->position.x, note.note_transforms[i]->position.y, init_note_depth);
			note.note_transforms[i]->scale = glm::vec3(0.0f, 0.0f, 0.0f);
		}
	}
}

// to_menu should be called either when the game is launched or when going from PLAYING -> PAUSED -> select EXIT
void PlayMode::to_menu() {
	// reset all state variables
	has_started = false;
	gameState = MENU;
	hovering_text = (uint8_t)chosen_song;

	reset_song();
	reset_cam();

	// stop currently playing song
	if (active_song) active_song->stop();
}

// start_song should only be called when going from MENU -> PLAYING or in restart_song
void PlayMode::start_song(int idx, bool restart) {
	if (has_started) return;

	reset_cam();
	SDL_SetRelativeMouseMode(SDL_TRUE);

	note_start_idx = 0;
	note_end_idx = 0;

	has_started = true;
	gameState = PLAYING;
	chosen_song = idx;

	music_start_time = std::chrono::high_resolution_clock::now(); // might want to reconsider if we want buffer time between starting the song and loading the level

	// choose the song based on index
	if (!restart) read_notes(song_list[idx].first);
	active_song = Sound::play(song_list[idx].second);
}

// restart_song should only be called when going from PLAYING -> PAUSED -> select RESTART
void PlayMode::restart_song() {
	reset_song();

	has_started = false;
	start_song(chosen_song, true);
}

// pause_song should only be called when going from PLAYING -> PAUSED
void PlayMode::pause_song() {
	// TODO : need to actually figure out how to pause song
	gameState = PAUSED;
	hovering_text = 0;
	music_pause_time = std::chrono::high_resolution_clock::now();
}

// unpause_song should only be called when going from PLAYING -> PAUSED -> select RESUME
void PlayMode::unpause_song() {
	// TODO : need to actually figure out how to unpause song
	gameState = PLAYING;
	SDL_SetRelativeMouseMode(SDL_TRUE);
	auto current_time = std::chrono::high_resolution_clock::now();
	music_start_time += current_time - music_pause_time;
}

void PlayMode::game_over(bool didClear) {
	if (didClear) {
		std::cout << "song cleared!\n";
	} else {
		std::cout << "song failed!\n";
	}
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_KEYDOWN) {
		if (gameState == MENU) {
			if (evt.key.keysym.sym == SDLK_RETURN) {
				start_song(hovering_text, false);
				return true;
			} else if (evt.key.keysym.sym == SDLK_UP) {
				hovering_text = hovering_text == 0 ? 0 : hovering_text - 1;
				return true;
			} else if (evt.key.keysym.sym == SDLK_DOWN) {
				hovering_text = hovering_text == static_cast<uint8_t>(song_list.size()) - 1? static_cast<uint8_t>(song_list.size()) - 1: hovering_text + 1;
				return true;
			} else if (evt.key.keysym.sym == SDLK_ESCAPE) {
				// press Exit key to close application, might want to change in future
				exit(0);
				return true;
			}
		} else if (gameState == PLAYING) {
			if (evt.key.keysym.sym == SDLK_z || evt.key.keysym.sym == SDLK_x) {
				check_hit();
				return true;
			} else if (evt.key.keysym.sym == SDLK_ESCAPE) {
				SDL_SetRelativeMouseMode(SDL_FALSE);
				pause_song();
				return true;
			}
		} else if (gameState == PAUSED) {
			if (evt.key.keysym.sym == SDLK_RETURN) {
				if (hovering_text == 0) {unpause_song(); return true;}
				else if (hovering_text == 1) {restart_song(); return true;}
				else if (hovering_text == 2) {to_menu(); return true;}
			}
			else if (evt.key.keysym.sym == SDLK_UP) {
				hovering_text = hovering_text == 0 ? 0 : hovering_text - 1;
				return true;
			} else if (evt.key.keysym.sym == SDLK_DOWN) {
				hovering_text = hovering_text == static_cast<uint8_t>(option_texts.size()) - 1? static_cast<uint8_t>(option_texts.size()) - 1: hovering_text + 1;
				return true;
			}if (evt.key.keysym.sym == SDLK_ESCAPE) {
				SDL_SetRelativeMouseMode(SDL_FALSE);
				unpause_song();
				return true;
			}
		}
	}
	else if (evt.type == SDL_MOUSEBUTTONDOWN) {
		SDL_SetRelativeMouseMode(SDL_TRUE);
		if (gameState != PLAYING) return true;
		check_hit();
	// 	hold = true;
	// } else if (evt.type == SDL_MOUSBUTTONUP) {
	// 	check_hit();
	// 	hold = false;
	} else if (evt.type == SDL_MOUSEMOTION) {
		if (gameState != PLAYING) return true;
		
		glm::vec2 delta;
		delta.x = evt.motion.xrel / float(window_size.x) * 2.0f;
		delta.x *= float(window_size.y) / float(window_size.x);
		delta.y = evt.motion.yrel / float(window_size.y) * -2.0f;

		cam.azimuth -= 0.4f * delta.x;
		cam.elevation -= 0.4f * delta.y;

		cam.azimuth /= 2.0f * 3.1415926f;
		cam.azimuth -= std::round(cam.azimuth);
		cam.azimuth *= 2.0f * 3.1415926f;

		cam.elevation /= 2.0f * 3.1415926f;
		cam.elevation -= std::round(cam.elevation);
		cam.elevation *= 2.0f * 3.1415926f;

		return true;
	}
	return false;
}

void PlayMode::update(float elapsed) {
	if (gameState == PLAYING) {
		update_notes();
	}
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	static std::array< glm::vec2, 16 > const circle = [](){
		std::array< glm::vec2, 16 > ret;
		for (uint32_t a = 0; a < ret.size(); ++a) {
			float ang = a / float(ret.size()) * 2.0f * float(M_PI);
			ret[a] = glm::vec2(std::cos(ang), std::sin(ang));
		}
		return ret;
	}();

	//update camera aspect ratio for drawable:
	camera->transform->rotation =
		normalize(glm::angleAxis(cam.azimuth, glm::vec3(0.0f, 1.0f, 0.0f))
		* glm::angleAxis(0.5f * 3.1415926f + -cam.elevation, glm::vec3(1.0f, 0.0f, 0.0f)))
	;
	camera->transform->scale = glm::vec3(1.0f);
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
		lines.draw_text("Up/down to navigate; enter to select; LMB to shoot",
			glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		float ofs = 2.0f / drawable_size.y;
		lines.draw_text("Up/down to navigate; enter to select; LMB to shoot",
			glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + + 0.1f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));

		if (gameState == MENU) {
			for (int i = hovering_text - 2; i < hovering_text + 3; i++) {
				if (i < 0 || i >= (int)song_list.size()) continue;
				// todo : these offsets need to be fixed...
				std::string text = song_list[i].first;
				if (i == hovering_text) {
					text = "-> " + text;
				}
				lines.draw_text(text, 
					glm::vec3(-aspect + 0.3f + ofs, (hovering_text - i) * 0.2f, 0.0f),
					glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
					glm::u8vec4(0xff, 0xff, 0xff, 0x00));
			}
		} else if (gameState == PLAYING) {
			// crosshair
			for (uint32_t a = 0; a < circle.size(); ++a) {
				for (float r = 0.02f; r >= 0.015f; r -= 0.001f) {
					lines.draw(
						glm::vec3(camera->transform->position.x + r * circle[a], 0.0f),
						glm::vec3(camera->transform->position.y + r * circle[(a+1)%circle.size()], 0.0f),
						glm::u8vec4(0xff, 0x00, 0x00, 0x00)
					);
				}
			}
		} else if (gameState == PAUSED) {
			for (int i = hovering_text - 2; i < hovering_text + 3; i++) {
				if (i < 0 || i >= (int)option_texts.size()) continue;
				// todo : these offsets need to be fixed...
				std::string text = option_texts[i];
				if (i == hovering_text) {
					text = "-> " + text;
				}
				lines.draw_text(text, 
					glm::vec3(-aspect + 0.3f + ofs, (hovering_text - i) * 0.2f, 0.0f),
					glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
					glm::u8vec4(0xff, 0xff, 0xff, 0x00));
			}
		}
	}
	GL_ERRORS();
}
