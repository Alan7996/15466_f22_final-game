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
	float const bgscale = abs(init_note_depth - max_depth);

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
		} else if (d.transform->name == "BGCenter") {
			d.transform->position = glm::vec3(0, 0, init_note_depth + 5.0f);
		} else if (d.transform->name == "BGUp") {
			d.transform->position = glm::vec3(0, y_scale, bgscale / 2.0f);
			d.transform->scale = glm::vec3(bgscale, bgscale, 1);
			backgrounds.emplace_back(&d);
		} else if (d.transform->name == "BGUp2") {
			d.transform->position = glm::vec3(0, y_scale, init_note_depth);
			d.transform->scale = glm::vec3(bgscale, bgscale, 1);
			backgrounds.emplace_back(&d);
		} else if (d.transform->name == "BGDown") {
			d.transform->position = glm::vec3(0, -y_scale, bgscale / 2.0f);
			d.transform->scale = glm::vec3(bgscale, bgscale, 1);
			backgrounds.emplace_back(&d);
		} else if (d.transform->name == "BGDown2") {
			d.transform->position = glm::vec3(0, -y_scale, init_note_depth);
			d.transform->scale = glm::vec3(bgscale, bgscale, 1);
			backgrounds.emplace_back(&d);
		} else if (d.transform->name == "BGLeft") {
			d.transform->position = glm::vec3(-x_scale, 0, bgscale / 2.0f);
			d.transform->scale = glm::vec3(bgscale, 1, bgscale);
			backgrounds.emplace_back(&d);
		} else if (d.transform->name == "BGLeft2") {
			d.transform->position = glm::vec3(-x_scale, 0, init_note_depth);
			d.transform->scale = glm::vec3(bgscale, 1, bgscale);
			backgrounds.emplace_back(&d);
		} else if (d.transform->name == "BGRight") {
			d.transform->position = glm::vec3(x_scale, 0, bgscale / 2.0f);
			d.transform->scale = glm::vec3(bgscale, 1, bgscale);
			backgrounds.emplace_back(&d);
		} else if (d.transform->name == "BGRight2") {
			d.transform->position = glm::vec3(x_scale, 0, init_note_depth);
			d.transform->scale = glm::vec3(bgscale, 1, bgscale);
			backgrounds.emplace_back(&d);
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
		x = -x_scale;
		y = coord;
	} else if (dir == "right") {
		x = x_scale;
		y = coord;
	} else if (dir == "up") {
		x = coord;
		y = y_scale;
	} else if (dir == "down") {
		x = coord;
		y = -y_scale;
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
			note.dir = dir;
			// this requires a bit more thinking on how to handle hold notes

			if (note_type == "hold") {
				// TODO: instead of making one note with a bunch of transforms, maybe consider making a bunch of notes with one transform each?

				note.noteType = NoteType::HOLD;

				for (int i = 0; i < idx - 4; i++) {
					float coord_begin = std::stof(note_info[3+i]);
					float time_begin = std::stof(note_info[idx+1+i]);

					float coord_end = std::stof(note_info[3+i+1]);
					float time_end = std::stof(note_info[idx+1+i+1]);
					note.coord_begin = coord_begin;
					note.coord_end = coord_end;
					std::pair<float, float> coords_begin = get_coords(dir, coord_begin);
					std::pair<float, float> coords_end = get_coords(dir, coord_end);

					Scene::Transform *transform = new Scene::Transform;
					transform->name = "Note";
					transform->position = glm::vec3((coords_begin.first + coords_end.first) / 2.0f, (coords_begin.second + coords_end.second) / 2.0f, init_note_depth);
					transform->scale = glm::vec3(0.0f, 0.0f, 0.0f); // all notes start from being invisible
					float angle = 0.0f;
					// if the xs are the same
					if(coords_begin.first == coords_end.first) {
						angle = -atan2((coords_begin.second - coords_end.second) / 2.0f, time_end - time_begin);
						transform->rotation = normalize(glm::angleAxis(angle, glm::vec3(1.0f, 0.0f, 0.0f)));
					}
					// otherwise the ys are the same
					else {
						angle = atan2((coords_begin.first - coords_end.first) / 2.0f, time_end - time_begin);
						transform->rotation = normalize(glm::angleAxis(angle, glm::vec3(0.0f, 1.0f, 0.0f)));
					}

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
				transform->rotation = (dir == "left" || dir == "right") ? glm::quat(1.0f, 0.0f, 0.0f, 0.0f) : glm::quat(0.7071f, 0.0f, 0.0f, 0.7071f);

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


void PlayMode::update_bg(float elapsed) {
	assert(gameState == PLAYING);
	// printf("%f", elapsed);
	// std::cout << " wat" << std::endl;

	// float note_speed = (border_depth - init_note_depth) / note_approach_time;
	// for (int i = 0; i < backgrounds.size(); i++) {
	// 	backgrounds[i]->transform->position.z = backgrounds[i]->transform->position.z + note_speed * elapsed;
	// 	if (backgrounds[i]->transform->position.z > 25.0f) backgrounds[i]->transform->position.z = init_note_depth;
	// }

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
	assert(gameState == PLAYING);

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
						if(note.noteType == NoteType::HOLD) {
							note.note_transforms[j]->scale = glm::vec3(0.1f, 0.1f, note.hit_times[j+1] - note.hit_times[j]);
							note.note_transforms[j]->position.z = init_note_depth - (note.hit_times[j+1] - note.hit_times[j]) / 2;
						}
						else {
							note.note_transforms[j]->scale = glm::vec3(0.1f, 0.1f, 0.1f);
						}
						note_end_idx += 1;
					} else {
						continue;
					}
				}
				else {
					note.note_transforms[j]->scale = glm::vec3(0.0f, 0.0f, 0.0f);
					note_start_idx += 1;

					if (note_start_idx == (int)notes.size()) game_over(true);
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
	if(note->noteType == NoteType::HOLD) {
		// at the moment, only one transform for hold

	}
	else {
		note->note_transforms[0]->scale = glm::vec3(0.0f, 0.0f, 0.0f);
	}
	// increment score & health

}

// AABB hit from https://gamedev.stackexchange.com/questions/18436/most-efficient-aabb-vs-ray-collision-algorithms/18459#18459
bool PlayMode::bbox_intersect(glm::vec3 pos, glm::vec3 dir, glm::vec3 min, glm::vec3 max, float &t) 
{ 
	// r.dir is unit direction vector of ray
	glm::vec3 dirfrac;
	dirfrac.x = 1.0f / dir.x;
	dirfrac.y = 1.0f / dir.y;
	dirfrac.z = 1.0f / dir.z;
	// min is the corner of AABB with minimal coordinates - left bottom, ma is maximal corner
	// pos is origin of ray
	float t1 = (min.x - pos.x)*dirfrac.x;
	float t2 = (max.x - pos.x)*dirfrac.x;
	float t3 = (min.y - pos.y)*dirfrac.y;
	float t4 = (max.y - pos.y)*dirfrac.y;
	float t5 = (min.z - pos.z)*dirfrac.z;
	float t6 = (max.z - pos.z)*dirfrac.z;

	float tmin = std::max(std::max(std::min(t1, t2), std::min(t3, t4)), std::min(t5, t6));
	float tmax = std::min(std::min(std::max(t1, t2), std::max(t3, t4)), std::max(t5, t6));

	// if tmax < 0, ray (line) is intersecting AABB, but the whole AABB is behind us
	if (tmax < 0)
	{
		t = tmax;
		return false;
	}

	// if tstd::min > tmax, ray doesn't intersect AABB
	if (tmin > tmax)
	{
		t = tmax;
		return false;
	}
	// std::cout << tmin << " " << tmax << "\n"; 
	t = (tmin + tmax) / 2.f;
	return true;
}

// attempt bounding box implementation
// currently using the same code three times, maybe think about what would actually be different between the three?
HitInfo PlayMode::trace_ray(glm::vec3 pos, glm::vec3 dir) {
	for (int i = note_start_idx; i < note_end_idx; i++) {
		NoteInfo &note = notes[i];
		// single type
		if (note.noteType == NoteType::SINGLE) {
			// get transform
			Scene::Transform *trans = note.note_transforms[0];
			// transform ray https://stackoverflow.com/questions/44630118/ray-transformation-in-a-ray-obb-intersection-test
			glm::mat4 inverse = trans->make_world_to_local();
			glm::vec4 start = inverse * glm::vec4(pos, 1.0);
			glm::vec4 direction = inverse * glm::vec4(dir, 0.0);
			direction = glm::normalize(direction);
			float t = 0.0f;
			// do bbox intersection
			if(bbox_intersect(start, direction, note.min, note.max, t)) {
				std::cout << "single\n";
				HitInfo hits;
				hits.note = &notes[i];
				hits.time = t;
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
			// transform ray https://stackoverflow.com/questions/44630118/ray-transformation-in-a-ray-obb-intersection-test
			glm::mat4 inverse = trans->make_world_to_local();
			glm::vec4 start = inverse * glm::vec4(pos, 1.0);
			glm::vec4 direction = inverse * glm::vec4(dir, 0.0);
			direction = glm::normalize(direction);
			// transform bounding box of note to world space
			float t = 0.0f;
			// do bbox intersection
			if(bbox_intersect(start, direction, note.min, note.max, t)) {
				std::cout << "burst\n";
				HitInfo hits;
				hits.note = &notes[i];
				hits.time = t;
				return hits;
			}
		}
		// hold type
		else if(note.noteType == NoteType::HOLD) {

			// assume we only have one hold per hold note

			Scene::Transform *trans = note.note_transforms[0];
			// transform ray https://stackoverflow.com/questions/44630118/ray-transformation-in-a-ray-obb-intersection-test
			glm::mat4 inverse = trans->make_world_to_local();
			glm::vec4 start = inverse * glm::vec4(pos, 1.0);
			glm::vec4 direction = inverse * glm::vec4(dir, 0.0);
			direction = glm::normalize(direction);
			// transform bounding box of note to world space
			float t = 0.0f;
			// do bbox intersection
			if(bbox_intersect(start, direction, note.min, note.max, t)) {
				// initial click (whether it's the start or not)
				std::cout << "hold\n";
				HitInfo hits;
				hits.note = &notes[i];
				hits.time = t;
				return hits;
			}
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
	ray = glm::normalize(glm::rotate(camera->transform->rotation, ray));
	// trace the ray to see if we hit a note
	HitInfo hits = trace_ray(camera->transform->position, ray);

	auto current_time = std::chrono::high_resolution_clock::now();
	float music_time = std::chrono::duration<float>(current_time - music_start_time).count();
	// if we hit a note, check to see if we hit a good time
	if(hits.note) {
		if(hits.note->noteType == NoteType::HOLD) {
			// only considers first 0.2 seconds of the hold -> need to change
			if(fabs(music_time - hits.note->hit_times[0] + real_song_offset) < valid_hit_time_delta && !holding) {
				// initial click
				score += 50;
				std::cout << "score: " << score << ", first click hitting\n";
			}
			else if(fabs(hits.note->hit_times[1] - music_time + real_song_offset) < valid_hit_time_delta && !holding) {
				// release near the end
				score += 50;
				std::cout << "score: " << score << ", last click hitting\n";
			}
			else if(holding) {
				// holding in between note
				// TODO: fix the math on the next line
				std::pair<float, float> coord = get_coords(hits.note->dir, hits.note->coord_begin + (music_time - hits.note->hit_times[0] + real_song_offset) * (hits.note->coord_end - hits.note->coord_begin) * note_approach_time / (hits.note->hit_times[1] - hits.note->hit_times[0]));
				glm::mat4 inverse = hits.note->note_transforms[0]->make_world_to_local();
				glm::vec4 start = inverse * glm::vec4(camera->transform->position, 1.0f);
				// std::cout << coord.first << " " << coord.second << " " << music_time << " " << hits.note->hit_times[0] + real_song_offset << "\n";
				glm::vec4 end = inverse * glm::vec4(coord.first, coord.second, border_depth, 1.0f);
				float dist = glm::distance(start, end);
				if(dist - 0.2 < hits.time && hits.time < dist + 0.2) {
					score += 10;
					std::cout << "score: " << score << ", still hitting\n";		
				}
				else {	
					score -= 20;
					std::cout << "score: " << score << ", missed hold\n";	
				}
				// std::cout << "dist: " << dist << ", time: " << hits.time << "\n";
			}
		}
		else {
			// valid hit time for single and burst
			// std::cout << music_time << " " << hits.note->hit_times[0] + real_song_offset << "\n";
			if(fabs(music_time - hits.note->hit_times[0] + real_song_offset) < valid_hit_time_delta) {
				score += 100;
				std::cout << "score: " << score << ", valid hit\n";
				hit_note(hits.note);
			}
			else {
				score -= 20;
				std::cout << "score: " << score << ", bad hit\n";
			}
		}
	}
	else {
		score -= 50;
		std::cout << "score: " << score << ", miss\n";
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
		if (evt.key.keysym.sym == SDLK_EQUALS) {
			mouse_sens = (mouse_sens + mouse_sens_inc >= mouse_sens_max) ? mouse_sens_max : mouse_sens + mouse_sens_inc;
			return true;
		} else if (evt.key.keysym.sym == SDLK_MINUS) {
			mouse_sens = (mouse_sens - mouse_sens_inc <= mouse_sens_min) ? mouse_sens_min : mouse_sens - mouse_sens_inc;
			return true;
		} else if (gameState == MENU) {
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
		holding = true;
	} else if (evt.type == SDL_MOUSEBUTTONUP) {
		if (gameState != PLAYING) return true;
		holding = false;
		check_hit();
	} else if (evt.type == SDL_MOUSEMOTION) {
		if (gameState != PLAYING) return true;
		
		glm::vec2 delta;
		delta.x = evt.motion.xrel / float(window_size.x) * 2.0f;
		delta.x *= float(window_size.y) / float(window_size.x);
		delta.y = evt.motion.yrel / float(window_size.y) * -2.0f;

		cam.azimuth -= mouse_sens * delta.x;
		cam.elevation -= mouse_sens * delta.y;

		cam.azimuth /= 2.0f * 3.1415926f;
		cam.azimuth -= std::round(cam.azimuth);
		cam.azimuth *= 2.0f * 3.1415926f;

		cam.elevation /= 2.0f * 3.1415926f;
		cam.elevation -= std::round(cam.elevation);
		cam.elevation *= 2.0f * 3.1415926f;
		//update camera aspect ratio for drawable:
		camera->transform->rotation =
			normalize(glm::angleAxis(cam.azimuth, glm::vec3(0.0f, 1.0f, 0.0f))
			* glm::angleAxis(0.5f * 3.1415926f + -cam.elevation, glm::vec3(1.0f, 0.0f, 0.0f)))
		;
		camera->transform->scale = glm::vec3(1.0f);
		if (holding) {
			check_hit();
		}
		return true;
	} else if (holding) {
		if (gameState != PLAYING) return true;
		check_hit();
		return true;
	}
	return false;
}

void PlayMode::update(float elapsed) {
	if (gameState == PLAYING) {
		update_bg(elapsed);
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
