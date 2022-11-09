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

// float representing a small epsilon
static float constexpr EPS_F = 0.0000001f;

// initialize the index to look up meshes info
GLuint main_meshes_for_lit_color_texture_program = 0;

/*
	Load in mesh data from main.pnct into meshbuffer and make the program
*/
Load< MeshBuffer > main_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("main.pnct"));
	main_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

/*
	Load in the scene data from main.scene and set up drawables vector with everything in the scene
*/
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

/*
	Load in song to play when we hit a note
*/
Load< Sound::Sample > note_hit(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("Note_hit.wav"));
});

/*
	Load in song to play when we miss a note
*/
Load< Sound::Sample > note_miss(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("Note_miss.wav"));
});

/*
	Load in song to play from Tutorial.wav
	
	// TODO: Would like to generalize this load_song function to take in string input and load string.wav file
*/
Load< Sound::Sample > load_song_tutorial(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("Tutorial.wav"));
});

/*
	Constructor for PlayMode
	Initialization of the game
		This involves the following:
			Set up the camera
			Go through all the drawables objects and save the values necessary to create a new object for the future
			Clear out the initial drawables vector so we have an empty scene
			Initialize the gun we carry
			Initialize the border where we will be hitting the notes
			Initialize the background scrolling
			Initialize list of songs to play for each level (at the moment, only one)
*/ 
PlayMode::PlayMode() : scene(*main_scene), note_hit_sound(*note_hit), note_miss_sound(*note_miss) {
	SDL_SetRelativeMouseMode(SDL_TRUE);

	meshBuf = new MeshBuffer(data_path("main.pnct"));

	// camera and assets
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();

	std::vector<Drawable> default_skin(15);
	std::vector<Drawable> backgrounds(9);

	for (auto &d : scene.drawables) {
		if (d.transform->name.find("Note") != std::string::npos) {
			// set up default skin array, only support total 10 skin variations
			int idx = d.transform->name.at(4) - '0';
			default_skin[idx].type = d.pipeline.type;
			default_skin[idx].start = d.pipeline.start;
			default_skin[idx].count = d.pipeline.count;
		} else if (d.transform->name == "Gun") {
			gun_drawable.type = d.pipeline.type;
			gun_drawable.start = d.pipeline.start;
			gun_drawable.count = d.pipeline.count;
		} else if (d.transform->name == "GunSingle") {
			gun_drawables[0].type = d.pipeline.type;
			gun_drawables[0].start = d.pipeline.start;
			gun_drawables[0].count = d.pipeline.count;
		} else if (d.transform->name == "GunBurst") {
			gun_drawables[1].type = d.pipeline.type;
			gun_drawables[1].start = d.pipeline.start;
			gun_drawables[1].count = d.pipeline.count;
		} else if (d.transform->name == "GunHold") {
			gun_drawables[2].type = d.pipeline.type;
			gun_drawables[2].start = d.pipeline.start;
			gun_drawables[2].count = d.pipeline.count;
		} else if (d.transform->name == "Border") {
			border_drawable.type = d.pipeline.type;
			border_drawable.start = d.pipeline.start;
			border_drawable.count = d.pipeline.count;
		} else if (d.transform->name == "BGCenter") {
			backgrounds[8].type = d.pipeline.type;
			backgrounds[8].start = d.pipeline.start;
			backgrounds[8].count = d.pipeline.count;
		} else if (d.transform->name == "BGUp") {
			backgrounds[0].type = d.pipeline.type;
			backgrounds[0].start = d.pipeline.start;
			backgrounds[0].count = d.pipeline.count;
		} else if (d.transform->name == "BGUp2") {
			backgrounds[1].type = d.pipeline.type;
			backgrounds[1].start = d.pipeline.start;
			backgrounds[1].count = d.pipeline.count;
		} else if (d.transform->name == "BGDown") {
			backgrounds[2].type = d.pipeline.type;
			backgrounds[2].start = d.pipeline.start;
			backgrounds[2].count = d.pipeline.count;
		} else if (d.transform->name == "BGDown2") {
			backgrounds[3].type = d.pipeline.type;
			backgrounds[3].start = d.pipeline.start;
			backgrounds[3].count = d.pipeline.count;
		} else if (d.transform->name == "BGLeft") {
			backgrounds[4].type = d.pipeline.type;
			backgrounds[4].start = d.pipeline.start;
			backgrounds[4].count = d.pipeline.count;
		} else if (d.transform->name == "BGLeft2") {
			backgrounds[5].type = d.pipeline.type;
			backgrounds[5].start = d.pipeline.start;
			backgrounds[5].count = d.pipeline.count;
		} else if (d.transform->name == "BGRight") {
			backgrounds[6].type = d.pipeline.type;
			backgrounds[6].start = d.pipeline.start;
			backgrounds[6].count = d.pipeline.count;
		} else if (d.transform->name == "BGRight2") {
			backgrounds[7].type = d.pipeline.type;
			backgrounds[7].start = d.pipeline.start;
			backgrounds[7].count = d.pipeline.count;
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
		gun_transform->scale = gun_scale;
		gun_transform->rotation = glm::quat(0.0f, 0.0f, 1.0f, 0.0f);
		scene.drawables.emplace_back(gun_transform);
		Scene::Drawable &d1 = scene.drawables.back();
		d1.pipeline = lit_color_texture_program_pipeline;
		d1.pipeline.vao = main_meshes_for_lit_color_texture_program;
		d1.pipeline.type = gun_drawable.type;
		d1.pipeline.start = gun_drawable.start;
		d1.pipeline.count = gun_drawable.count;

		gun_transforms.resize(3);
		for (int i = 0; i < 3; i++) {
			gun_transforms[i] = new Scene::Transform;
			gun_transforms[i]->parent = camera->transform;
			// TODO: these numbers need tweaking once we finalize the gun model
			gun_transforms[i]->position = glm::vec3(0.03f, -0.06f, -0.4f);
			gun_transforms[i]->scale = glm::vec3();
			gun_transforms[i]->rotation = glm::quat(0.0f, 0.0f, 1.0f, 0.0f);
			scene.drawables.emplace_back(gun_transforms[i]);
		}

		// only draw SINGLE gun to start with
		// scene.drawables.emplace_back(gun_transforms[0]);
		// Scene::Drawable &d_gun = scene.drawables.back();
		// d_gun.pipeline = lit_color_texture_program_pipeline;
		// d_gun.pipeline.vao = main_meshes_for_lit_color_texture_program;
		// d_gun.pipeline.type = gun_drawables[0].type;
		// d_gun.pipeline.start = gun_drawables[0].start;
		// d_gun.pipeline.count = gun_drawables[0].count;

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

		bg_transforms.resize(9);
		bgscale = abs(init_note_depth - max_depth);

		for (int i = 0; i < 9; i++) {
			bg_transforms[i] = new Scene::Transform;
			switch (i) {
				case 0: // Up
					bg_transforms[i]->position = glm::vec3(0, 2.0f * y_scale, 0);
					bg_transforms[i]->scale = glm::vec3(bgscale, 1, bgscale);
					break;
				case 1:
					bg_transforms[i]->position = glm::vec3(0, 2.0f * y_scale, -2.0f * bgscale);
					bg_transforms[i]->scale = glm::vec3(bgscale, 1, bgscale);
					break;
				case 2: // Down
					bg_transforms[i]->position = glm::vec3(0, -2.0f * y_scale, 0);
					bg_transforms[i]->scale = glm::vec3(bgscale, 1, bgscale);
					break;
				case 3:
					bg_transforms[i]->position = glm::vec3(0, -2.0f * y_scale, -2.0f * bgscale);
					bg_transforms[i]->scale = glm::vec3(bgscale, 1, bgscale);
					break;
				case 4: // Left
					bg_transforms[i]->position = glm::vec3(-2.0f * x_scale, 0, 0);
					bg_transforms[i]->scale = glm::vec3(1, bgscale, bgscale);
					break;
				case 5:
					bg_transforms[i]->position = glm::vec3(-2.0f * x_scale, 0, -2.0f * bgscale);
					bg_transforms[i]->scale = glm::vec3(1, bgscale, bgscale);
					break;
				case 6: // Right
					bg_transforms[i]->position = glm::vec3(2.0f * x_scale, 0, 0);
					bg_transforms[i]->scale = glm::vec3(1, bgscale, bgscale);
					break;
				case 7:
					bg_transforms[i]->position = glm::vec3(2.0f * x_scale, 0, -2.0f * bgscale);
					bg_transforms[i]->scale = glm::vec3(1, bgscale, bgscale);
					break;
				case 8: // Center
					bg_transforms[i]->position = glm::vec3(0, 0, -bgscale);
					bg_transforms[i]->scale = glm::vec3(bgscale, bgscale, 1);
					break;
			}
			scene.drawables.emplace_back(bg_transforms[i]);
			Scene::Drawable &d_bg = scene.drawables.back();
			d_bg.pipeline = lit_color_texture_program_pipeline;
			d_bg.pipeline.vao = main_meshes_for_lit_color_texture_program;
			d_bg.pipeline.type = backgrounds[i].type;
			d_bg.pipeline.start = backgrounds[i].start;
			d_bg.pipeline.count = backgrounds[i].count;
		}

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

/*
	Destructor for PlayMode
*/
PlayMode::~PlayMode() {
}

/*
	Helper function that takes in a string and delimiter and returns a vector of strings formed by splitting by delimiter
	Reference from https://java2blog.com/split-string-space-cpp/
*/
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

/*
	Helper function that returns the coordinates on the border given a direction and a value
		Dir is a string representing a direction
		Coord should range from -1 to 1, going from left to right and bottom to top.
*/ 

glm::vec2 PlayMode::get_coords(std::string dir, float coord) {
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
	return glm::vec2(x, y);
}

/*
Reads a txt file representing a beatmap and fills in the proper data structures to correctly render on screen
	Each line of the txt file represents one note (single, burst, hold)
	In the format of <type> <skin number> <direction> <coord begin> <coord end> @ <time begin> <time end>
	Constructs a NoteInfo for each line and pushes it into the vector of notes
		*** Each line must have its time begin be less than the time begins of all lines after it ***


	// TODO: instead of making one hold note with a bunch of transforms, maybe consider making a bunch of notes with one transform each?
*/
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
				note.noteType = NoteType::HOLD;

				for (int i = 0; i < idx - 4; i++) {
					float coord_begin = std::stof(note_info[3+i]);
					float time_begin = std::stof(note_info[idx+1+i]);

					float coord_end = std::stof(note_info[3+i+1]);
					float time_end = std::stof(note_info[idx+1+i+1]);
					note.coord_begin = coord_begin;
					note.coord_end = coord_end;
					glm::vec2 coords_begin = get_coords(dir, coord_begin);
					glm::vec2 coords_end = get_coords(dir, coord_end);

					Scene::Transform *transform = new Scene::Transform;
					transform->name = "Note";
					transform->position = glm::vec3((coords_begin.x + coords_end.x) / 2.0f, (coords_begin.y + coords_end.y) / 2.0f, init_note_depth);
					transform->scale = glm::vec3(0.0f, 0.0f, 0.0f); // all notes start from being invisible
					float angle = 0.0f;
					// if the xs are the same
					if(coords_begin.x == coords_end.x) {
						angle = -atan2((coords_begin.y - coords_end.y) / 2.0f, time_end - time_begin);
						transform->rotation = normalize(glm::angleAxis(angle, glm::vec3(1.0f, 0.0f, 0.0f)));
					}
					// otherwise the ys are the same
					else {
						angle = atan2((coords_begin.x - coords_end.x) / 2.0f, time_end - time_begin);
						transform->rotation = normalize(glm::angleAxis(angle, glm::vec3(0.0f, 1.0f, 0.0f)));
					}

					note.note_transforms.push_back(transform);
					note.hit_times.push_back(time_begin);
					note.hit_times.push_back(time_end);
				}
			} else {
				float coord = std::stof(note_info[3]);
				float time = std::stof(note_info[5]);
				glm::vec2 coords = get_coords(dir, coord);
				
				note.noteType = note_type == "single" ? NoteType::SINGLE : NoteType::BURST;

				Scene::Transform *transform = new Scene::Transform;
				transform->name = "Note";
				transform->position = glm::vec3(coords.x, coords.y, init_note_depth);
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

/*
	Helper function to create a scroll effect on the background
	Switches between two backgrounds on each direction to create an effect of it being infinite
*/
void PlayMode::update_bg(float elapsed) {
	assert(game_state == PLAYING);

	float note_speed = (border_depth - init_note_depth) / note_approach_time;
	for (size_t i = 0; i < bg_transforms.size() - 1; i++) {
		bg_transforms[i]->position.z = bg_transforms[i]->position.z + note_speed * elapsed;
		if (bg_transforms[i]->position.z > 2.0f * bgscale) 
			bg_transforms[i]->position.z = -2.0f * bgscale;
	}

}

/* 
	We maintain the list of notes to be checked in the following way: 
		We keep track of two types of variables. First are note_start_idx and 
		note_end_idx, which records the range of notes that have spawned but have
		yet to reach the disappearing line. Second are each note's is_active boolean,
		which if a note was correctly hit by the player should toggle to false and
		make the note have 0 scale. We however do not immediately update the indices,
		meaning the note will continue to move towards the player until it reaches the
		disappearing line. This is so that we can keep a nice loop from start to end
		indices without worrying about tight time gaps between notes. 

	Also note that we always loop up to ONE INDEX HIGHER than the end index, to
	check if we should start the next note or not.
*/
void PlayMode::update_notes() {
	assert(game_state == PLAYING);

	auto current_time = std::chrono::high_resolution_clock::now();
	float music_time = std::chrono::duration<float>(current_time - music_start_time).count();
	
	for (int i = note_start_idx; i < note_end_idx + 1; i++) {
		if (i >= (int)notes.size()) continue;
		auto &note = notes[i];
		for (int j = 0; j < (int)note.note_transforms.size(); j++) {
			if (note.is_active) {
				if (music_time > note.hit_times[j] + valid_hit_time_delta + real_song_offset) {
					// 'delete' the note
					if (!note.been_hit) hit_note(nullptr, -1);

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
				if (!note.been_hit) {
					if (music_time >= note.hit_times[j] - note_approach_time + real_song_offset) {
						// spawn the note
						note.is_active = true;
						if(note.noteType == NoteType::HOLD) {
							note.note_transforms[j]->scale = glm::vec3(0.1f, 0.1f, note.hit_times[j+1] - note.hit_times[j]);
							note.note_transforms[j]->position.z = init_note_depth - (note.hit_times[j+1] - note.hit_times[j]) / 2;
						}
						else {
							note.note_transforms[j]->scale = glm::vec3(0.1f, 0.1f, 0.1f);
						}
						note_end_idx += 1;
					}
				}
			}
		}
	}
}

/*
	Helper function to run AABB intersection
	Reference from https://gamedev.stackexchange.com/questions/18436/most-efficient-aabb-vs-ray-collision-algorithms/18459#18459
*/
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

/*
	Helper function to trace a ray from pos in dir direction against every visible note
		Transforms both pos and dir to note's local space in order to take care of OBB

	// TODO: At the moment, uses the same code for all three types
			 Need to consider what would actually be different between the three
*/
// currently using the same code three times, maybe think about what would actually be different between the three?
HitInfo PlayMode::trace_ray(glm::vec3 pos, glm::vec3 dir) {
	for (int i = note_start_idx; i < note_end_idx; i++) {
		NoteInfo &note = notes[i];

		// TODO : might want to revisit this as extension for HOLD notes
		if (note.note_transforms[0]->scale == glm::vec3()) continue;

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

/*
	Update function to a note, score / combo and health
*/
void PlayMode::hit_note(NoteInfo* note, int hit_status) {
	if (hit_status == -1) {
		health = std::max(0.0f, health - 0.1f);
		combo = 0;
		if (health < EPS_F) game_over(false);
		return;
	}

	// deactivate the note
	note->been_hit = true;

	if (hit_status == 0) {
		// bad hit, same as miss
		Sound::play(note_miss_sound);
		combo = 0;
		health = std::max(0.0f, health - 0.1f);
		if (health < EPS_F) game_over(false);
	} else if (hit_status == 1) {
		// ok hit
		Sound::play(note_hit_sound);
		score += 50;
		combo += 1;
		multiplier = combo / 25 + 1;
		health = std::min(max_health, health + 0.03f);
	} else if (hit_status == 2) {
		// good hit
		Sound::play(note_hit_sound);
		score += 100;
		combo += 1;
		multiplier = combo / 25 + 1;
		health = std::min(max_health, health + 0.03f);
	}

	// TODO: fix this for hold
	if(note->noteType == NoteType::HOLD) {
		// at the moment, only one transform for hold

	}
	else {
		note->note_transforms[0]->scale = glm::vec3(0.0f, 0.0f, 0.0f);
	}
}

/*
	Function called whenever we click on the screen
		Checks if we hit any note, see if the hit is valid or not and then calls hit note based on that
*/
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
				glm::vec2 coord = get_coords(hits.note->dir, hits.note->coord_begin + (music_time - hits.note->hit_times[0] + real_song_offset) * (hits.note->coord_end - hits.note->coord_begin) * note_approach_time / (hits.note->hit_times[1] - hits.note->hit_times[0]));
				glm::mat4 inverse = hits.note->note_transforms[0]->make_world_to_local();
				glm::vec3 start = glm::vec3(inverse * glm::vec4(camera->transform->position, 1.0f));
				// std::cout << coord.first << " " << coord.second << " " << music_time << " " << hits.note->hit_times[0] + real_song_offset << "\n";
				glm::vec3 end = glm::vec3(inverse * glm::vec4(coord.x, coord.y, border_depth, 1.0f));
				float dist = glm::distance(start, end);
				if(dist - 2 < hits.time && hits.time < dist + 2) {
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
			if(fabs(music_time - hits.note->hit_times[0] + real_song_offset) < valid_hit_time_delta / 2.0f) {
				// good hit
				hit_note(hits.note, 2);
				std::cout << "score: " << score << ", good hit\n";
			} else if (fabs(music_time - hits.note->hit_times[0] + real_song_offset) < valid_hit_time_delta) {
				// ok hit
				hit_note(hits.note, 1);
				std::cout << "score: " << score << ", ok hit\n";
			}
			else {
				// bad hit
				hit_note(hits.note, 0);
				std::cout << "score: " << score << ", bad hit\n";
			}
		}
	}
	else {
		// miss
	}
}

/*
	Helper function to change gun firing mode and relevant drawable / transform settings
*/
void PlayMode::change_gun(int idx_change, int manual_idx=-1) {
	assert(manual_idx != -1 || (idx_change == 1 || idx_change == -1));

	if (manual_idx != -1) {
		gun_mode = manual_idx;
		gun_transforms[manual_idx]->scale = gun_scale;
	} else {
		gun_transforms[gun_mode]->scale = glm::vec3();

		gun_mode += idx_change;
		if (gun_mode == 3) gun_mode = 0;
		else if (gun_mode == -1) gun_mode = 2;

		gun_transforms[gun_mode]->scale = gun_scale;
	}
}

/*
	Helper function to resetting all note values to initial state when restarting a song
*/
void PlayMode::reset_song() {
	// reset loaded assets
	if (active_song) active_song->stop();
	for (auto &note: notes) {
		note.been_hit = false;
		note.is_active = false;
		for (uint64_t i = 0; i < note.note_transforms.size(); i++) {
			note.note_transforms[i]->position = glm::vec3(note.note_transforms[i]->position.x, note.note_transforms[i]->position.y, init_note_depth);
			note.note_transforms[i]->scale = glm::vec3(0.0f, 0.0f, 0.0f);
		}
	}
}


/*
	Function that changes the game state to the default menu
		to_menu should only be called either when the game is launched or when going from PLAYING -> PAUSED -> select EXIT

*/
void PlayMode::to_menu() {
	// reset all state variables
	has_started = false;
	game_state = MENU;
	hovering_text = (uint8_t)chosen_song;

	reset_song();
	reset_cam();

	// stop currently playing song
	if (active_song) active_song->stop();
}


/*
	Function that starts a song (and initializes all variables) when we choose to start a level
		start_song should only be called when going from MENU -> PLAYING or in restart_song
*/
void PlayMode::start_song(int idx, bool restart) {
	if (has_started) return;

	reset_cam();
	SDL_SetRelativeMouseMode(SDL_TRUE);

	note_start_idx = 0;
	note_end_idx = 0;
	score = 0;
	combo = 0;
	multiplier = 1;
	health = 0.7f;

	change_gun(0, 0);

	has_started = true;
	game_state = PLAYING;
	chosen_song = idx;

	music_start_time = std::chrono::high_resolution_clock::now(); // might want to reconsider if we want buffer time between starting the song and loading the level

	// choose the song based on index
	if (!restart) read_notes(song_list[idx].first);
	active_song = Sound::play(song_list[idx].second);
}

/*
	Function that restarts a song (and reinitializes all variables) when we choose to restart a level
		restart_song should only be called when going from PLAYING -> PAUSED -> select RESTARTg
*/
void PlayMode::restart_song() {
	reset_song();

	has_started = false;
	start_song(chosen_song, true);
}

/*
	Function that pauses the game
		pause_song should only be called when going from PLAYING -> PAUSED

	// TODO : need to actually figure out how to pause song
*/
void PlayMode::pause_song() {
	game_state = PAUSED;
	hovering_text = 0;
	music_pause_time = std::chrono::high_resolution_clock::now();
}

/*
	Function that unpauses the game
		unpause_song should only be called when going from PLAYING -> PAUSED -> select RESUME

	// TODO : need to actually figure out how to unpause song
*/
void PlayMode::unpause_song() {
	game_state = PLAYING;
	SDL_SetRelativeMouseMode(SDL_TRUE);
	auto current_time = std::chrono::high_resolution_clock::now();
	music_start_time += current_time - music_pause_time;
}

/*
	Function that ends the game and makes all events do nothing
		Should be called when either health reaches zero or if note_start_idx is equal to the end of the vector 
*/
void PlayMode::game_over(bool did_clear) {
	hovering_text = 0;
	if (did_clear) {
		std::cout << "song cleared!\n";
		game_state = SONGCLEAR;
	} else {
		std::cout << "song failed!\n";
		game_state = GAMEOVER;
	}
}

/*
	Function that handles a SDL event and cases on what to do as a result
*/
bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_KEYDOWN) {
		if (evt.key.keysym.sym == SDLK_EQUALS) {
			mouse_sens = (mouse_sens + mouse_sens_inc >= mouse_sens_max) ? mouse_sens_max : mouse_sens + mouse_sens_inc;
			return true;
		} else if (evt.key.keysym.sym == SDLK_MINUS) {
			mouse_sens = (mouse_sens - mouse_sens_inc <= mouse_sens_min) ? mouse_sens_min : mouse_sens - mouse_sens_inc;
			return true;
		} else if (game_state == MENU) {
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
		} else if (game_state == PLAYING) {
			if (evt.key.keysym.sym == SDLK_c || evt.key.keysym.sym == SDLK_v) {
				check_hit();
				return true;
			} else if (evt.key.keysym.sym == SDLK_z) {
				// go to previous gun mode
				change_gun(-1);
				return true;
			} else if (evt.key.keysym.sym == SDLK_x) {
				// go to next gun mode
				change_gun(1);
				return true;
			} else if (evt.key.keysym.sym == SDLK_ESCAPE) {
				SDL_SetRelativeMouseMode(SDL_FALSE);
				pause_song();
				return true;
			}
		} else if (game_state == PAUSED) {
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
		} else if (game_state == SONGCLEAR || game_state == GAMEOVER) {
			if (evt.key.keysym.sym == SDLK_RETURN) {
				if (hovering_text == 0) {restart_song(); return true;}
				else if (hovering_text == 1) {to_menu(); return true;}
			}
			else if (evt.key.keysym.sym == SDLK_UP) {
				hovering_text = hovering_text == 0 ? 0 : hovering_text - 1;
				return true;
			} else if (evt.key.keysym.sym == SDLK_DOWN) {
				hovering_text = hovering_text == static_cast<uint8_t>(songover_texts.size()) - 1? static_cast<uint8_t>(songover_texts.size()) - 1: hovering_text + 1;
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
		if (game_state != PLAYING) return true;
		check_hit();
		holding = true;
	} else if (evt.type == SDL_MOUSEBUTTONUP) {
		if (game_state != PLAYING) return true;
		holding = false;
		check_hit();
	} else if (evt.type == SDL_MOUSEMOTION) {
		if (game_state != PLAYING) return true;
		
		// From ShowSceneProgram.cpp
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
		if (game_state != PLAYING) return true;
		check_hit();
		return true;
	}
	return false;
}

/*
	Function to call the update functions on the background and notes
*/
void PlayMode::update(float elapsed) {
	if (game_state == PLAYING) {
		update_bg(elapsed);
		update_notes();
	}
}

/*
	Function to draw the scene
*/
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
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 0);
	glUniform3fv(lit_color_texture_program->LIGHT_LOCATION_vec3, 1, glm::value_ptr(camera->transform->position + glm::vec3(0.0f, 0.0f, 0.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f, -1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(50.0f, 50.0f, 50.0f)));
	glUseProgram(0);

	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClearDepth(1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

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
		float ofs = 2.0f / drawable_size.y;

		if (game_state == MENU) {
			for (int i = hovering_text - 2; i < hovering_text + 3; i++) {
				if (i < 0 || i >= (int)song_list.size()) continue;
				std::string text = song_list[i].first;
				if (i == hovering_text) {
					text = "-> " + text;
				}
				lines.draw_text(text, 
					glm::vec3(-aspect + 0.3f + ofs, (hovering_text - i) * 0.2f, 0.0f),
					glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
					glm::u8vec4(0xff, 0xff, 0xff, 0x00));
			}
		} else if (game_state == PLAYING) {
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
			lines.draw_text(std::to_string(score),
				glm::vec3(aspect - 0.3f - ofs, 0.8f, 0.0f),
				glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
				glm::u8vec4(0xff, 0xff, 0xff, 0x00));
		} else if (game_state == PAUSED) {
			for (int i = hovering_text - 2; i < hovering_text + 3; i++) {
				if (i < 0 || i >= (int)option_texts.size()) continue;
				std::string text = option_texts[i];
				if (i == hovering_text) {
					text = "-> " + text;
				}
				lines.draw_text(text, 
					glm::vec3(-aspect + 0.3f + ofs, (hovering_text - i) * 0.2f, 0.0f),
					glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
					glm::u8vec4(0xff, 0xff, 0xff, 0x00));
			}
		} else if (game_state == SONGCLEAR) {
			for (int i = hovering_text - 2; i < hovering_text + 3; i++) {
				if (i < 0 || i >= (int)songover_texts.size()) continue;
				std::string text = songover_texts[i];
				if (i == hovering_text) {
					text = "-> " + text;
				}
				lines.draw_text(text, 
					glm::vec3(-aspect + 0.3f + ofs, (hovering_text - i) * 0.2f, 0.0f),
					glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
					glm::u8vec4(0xff, 0xff, 0xff, 0x00));
			}
		} else if (game_state == GAMEOVER) {
			for (int i = hovering_text - 2; i < hovering_text + 3; i++) {
				if (i < 0 || i >= (int)songover_texts.size()) continue;
				std::string text = songover_texts[i];
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
