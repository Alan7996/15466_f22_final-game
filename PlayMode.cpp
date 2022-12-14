// TODO : figure out why border is set to 2.5 (change lines ~260)
// TODO : fix intersection code for hold (I think burst is fine for now)
// TODO : currently, scaling and positioning hold incorrectly (might also be true for the burst and single)

#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"
#include "load_save_png.hpp"

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
	Load in sfx to play when we hit a note
*/
Load< Sound::Sample > note_hit(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("Note_hit.opus"));
});

/*
	Load in sfx to play when we miss a note
*/
Load< Sound::Sample > note_miss(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("Note_miss.opus"));
});

/*
	Load in songs to play from songs/ folder
*/
Load< Sound::Sample > load_song_tutorial(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("songs/Tutorial.opus"));
});

Load< Sound::Sample > load_song_the_beginning(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("songs/The Beginning.opus"));
});

Load< Sound::Sample > load_song_hellbound(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("songs/Hellbound.opus"));
});

Load< Sound::Sample > load_song_halloween_madness(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("songs/Halloween Madness.opus"));
});

Load< Sound::Sample > load_song_menu(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("Menu_background.opus"));
});

// From: https://github.com/ixchow/15-466-f18-base3/blob/586f23cf0bbaf80e8e70277442c4e0de7e7612f5/GameMode.cpp#L95-L113
GLuint load_texture(std::string const &filename) {
	glm::uvec2 size;
	std::vector< glm::u8vec4 > data;
	load_png(filename, &size, &data, LowerLeftOrigin);

	GLuint tex = 0;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, size.x, size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, data.data());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	glGenerateMipmap(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, 0);
	GL_ERRORS();

	return tex;
}

// Based on works by Hunan Express team (Dakota Hernandez)
Load< GLuint > load_tex_wall_left(LoadTagDefault, [](){
	return new GLuint(load_texture(data_path("textures/wall_left.png")));
});

Load< GLuint > load_tex_wall_right(LoadTagDefault, [](){
	return new GLuint(load_texture(data_path("textures/wall_right.png")));
});

Load< GLuint > load_tex_wall_up(LoadTagDefault, [](){
	return new GLuint(load_texture(data_path("textures/wall_up.png")));
});

Load< GLuint > load_tex_wall_down(LoadTagDefault, [](){
	return new GLuint(load_texture(data_path("textures/wall_down.png")));
});

Load< GLuint > load_tex_game_over(LoadTagDefault, [](){
	return new GLuint(load_texture(data_path("textures/game_over.png")));
});

Load< GLuint > load_tex_clear(LoadTagDefault, [](){
	return new GLuint(load_texture(data_path("textures/clear.png")));
});

/*
	Constructor for PlayMode
	Initialization of the game
		This involves the following (and more):
			Set up the camera
			Go through all the drawables objects and save the values necessary to create a new object for the future
			Clear out the initial drawables vector so we have an empty scene
			Initialize the gun we carry
			Initialize the border where we will be hitting the notes
			Initialize the background scrolling
			Initialize list of songs to play for each level (at the moment, only one)
*/ 
PlayMode::PlayMode() : scene(*main_scene), note_hit_sound(*note_hit), note_miss_sound(*note_miss) {

	game_state = GAMEOVER;

	meshBuf = new MeshBuffer(data_path("main.pnct"));

	// camera and assets
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();

	std::vector<Drawable> skin_halloween(10);
	std::vector<Drawable> skin_christmas(10);
	std::vector<Drawable> backgrounds(3);
	std::vector<Drawable> gun_drawables(3);

	for (auto &d : scene.drawables) {
		if (d.transform->name.find("NoteHalloween") != std::string::npos) {
			// store Halloween skin array, only support total 10 skin variations
			int idx = d.transform->name.at(13) - '0';
			skin_halloween[idx].type = d.pipeline.type;
			skin_halloween[idx].start = d.pipeline.start;
			skin_halloween[idx].count = d.pipeline.count;
		} else if (d.transform->name.find("NoteChristmas") != std::string::npos) {
			// store Christmas skin array, only support total 10 skin variations
			int idx = d.transform->name.at(13) - '0';
			skin_christmas[idx].type = d.pipeline.type;
			skin_christmas[idx].start = d.pipeline.start;
			skin_christmas[idx].count = d.pipeline.count;
		} else if (d.transform->name == "Perfect") {
			// store 3 different "hit" meshes
			hit_perfect.type = d.pipeline.type;
			hit_perfect.start = d.pipeline.start;
			hit_perfect.count = d.pipeline.count;
		} else if (d.transform->name == "Good") {
			hit_good.type = d.pipeline.type;
			hit_good.start = d.pipeline.start;
			hit_good.count = d.pipeline.count;
		} else if (d.transform->name == "Miss") {
			hit_miss.type = d.pipeline.type;
			hit_miss.start = d.pipeline.start;
			hit_miss.count = d.pipeline.count;
		} else if (d.transform->name == "GunSingle") {
			// store 3 different gun types
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
			// store health bar relevant meshes
			border_drawable.type = d.pipeline.type;
			border_drawable.start = d.pipeline.start;
			border_drawable.count = d.pipeline.count;
		} else if (d.transform->name == "HealthBar") {
			healthbar_drawable.type = d.pipeline.type;
			healthbar_drawable.start = d.pipeline.start;
			healthbar_drawable.count = d.pipeline.count;
		} else if (d.transform->name == "HealthBarLeft") {
			healthbarleft_drawable.type = d.pipeline.type;
			healthbarleft_drawable.start = d.pipeline.start;
			healthbarleft_drawable.count = d.pipeline.count;
		} else if (d.transform->name == "HealthBarRight") {
			healthbarright_drawable.type = d.pipeline.type;
			healthbarright_drawable.start = d.pipeline.start;
			healthbarright_drawable.count = d.pipeline.count;
		} else if (d.transform->name == "Health") {
			health_drawable.type = d.pipeline.type;
			health_drawable.start = d.pipeline.start;
			health_drawable.count = d.pipeline.count;
		} else if (d.transform->name == "BGCenter") {
			// store background wall meshes
			backgrounds[2].type = d.pipeline.type;
			backgrounds[2].start = d.pipeline.start;
			backgrounds[2].count = d.pipeline.count;
		} else if (d.transform->name == "BGDown") {
			backgrounds[0].type = d.pipeline.type;
			backgrounds[0].start = d.pipeline.start;
			backgrounds[0].count = d.pipeline.count;
		} else if (d.transform->name == "BGLeft") {
			backgrounds[1].type = d.pipeline.type;
			backgrounds[1].start = d.pipeline.start;
			backgrounds[1].count = d.pipeline.count;
		}
	}

	beatmap_skins.emplace_back(std::make_pair("NoteHalloween", skin_halloween));
	beatmap_skins.emplace_back(std::make_pair("NoteChristmas", skin_christmas));
	scene.drawables.clear();

	{ // initialize game state
		// set up gun meshes
		gun_transforms.resize(3);
		for (int i = 0; i < 3; i++) {
			gun_transforms[i] = new Scene::Transform;
			gun_transforms[i]->parent = camera->transform;
			gun_transforms[i]->position = glm::vec3(0.07f, -0.06f, -0.25f);
			gun_transforms[i]->scale = glm::vec3();

			scene.drawables.emplace_back(gun_transforms[i]);
			Scene::Drawable &d_gun = scene.drawables.back();
			d_gun.pipeline = lit_color_texture_program_pipeline;
			d_gun.pipeline.vao = main_meshes_for_lit_color_texture_program;
			d_gun.pipeline.type = gun_drawables[i].type;
			d_gun.pipeline.start = gun_drawables[i].start;
			d_gun.pipeline.count = gun_drawables[i].count;
		}

		// set up health bar meshes
		{
			healthbar_transform = new Scene::Transform;
			healthbar_transform->name = "Healthbar";
			healthbar_transform->parent = camera->transform;
			healthbar_transform->position = healthbar_position;
			scene.drawables.emplace_back(healthbar_transform);
			Scene::Drawable &d1 = scene.drawables.back();
			d1.pipeline = lit_color_texture_program_pipeline;
			d1.pipeline.vao = main_meshes_for_lit_color_texture_program;
			d1.pipeline.type = healthbar_drawable.type;
			d1.pipeline.start = healthbar_drawable.start;
			d1.pipeline.count = healthbar_drawable.count;

			healthbarleft_transform = new Scene::Transform;
			healthbarleft_transform->name = "HealthbarLeft";
			healthbarleft_transform->parent = healthbar_transform;
			scene.drawables.emplace_back(healthbarleft_transform);
			Scene::Drawable &d1l = scene.drawables.back();
			d1l.pipeline = lit_color_texture_program_pipeline;
			d1l.pipeline.vao = main_meshes_for_lit_color_texture_program;
			d1l.pipeline.type = healthbarleft_drawable.type;
			d1l.pipeline.start = healthbarleft_drawable.start;
			d1l.pipeline.count = healthbarleft_drawable.count;

			healthbarright_transform = new Scene::Transform;
			healthbarright_transform->name = "HealthbarRight";
			healthbarright_transform->parent = healthbar_transform;
			scene.drawables.emplace_back(healthbarright_transform);
			Scene::Drawable &d1r = scene.drawables.back();
			d1r.pipeline = lit_color_texture_program_pipeline;
			d1r.pipeline.vao = main_meshes_for_lit_color_texture_program;
			d1r.pipeline.type = healthbarright_drawable.type;
			d1r.pipeline.start = healthbarright_drawable.start;
			d1r.pipeline.count = healthbarright_drawable.count;

			health_transform = new Scene::Transform;
			health_transform->name = "Health";
			health_transform->parent = healthbar_transform;
			scene.drawables.emplace_back(health_transform);
			Scene::Drawable &d0 = scene.drawables.back();
			d0.pipeline = lit_color_texture_program_pipeline;
			d0.pipeline.vao = main_meshes_for_lit_color_texture_program;
			d0.pipeline.type = health_drawable.type;
			d0.pipeline.start = health_drawable.start;
			d0.pipeline.count = health_drawable.count;

			border_transform = new Scene::Transform;
			border_transform->name = "Border";
			border_transform->position = glm::vec3(0.0f, 0.0f, 2.5f);
			border_transform->scale = glm::vec3(x_scale, y_scale, 0.01f);
			scene.drawables.emplace_back(border_transform);
			Scene::Drawable &d2 = scene.drawables.back();
			d2.pipeline = lit_color_texture_program_pipeline;
			d2.pipeline.vao = main_meshes_for_lit_color_texture_program;
			d2.pipeline.type = border_drawable.type;
			d2.pipeline.start = border_drawable.start;
			d2.pipeline.count = border_drawable.count;
		}

		// set up background walls
		size_t num_walls = 2;
		bg_transforms.resize(4 * num_walls + 2);

		for (size_t i = 0; i < bg_transforms.size(); i++) {
			GLuint tex_ind = 0;

			bg_transforms[i] = new Scene::Transform;
			switch (i) {
				case 0: // Up
					bg_transforms[i]->position = glm::vec3(0, 5.0f * y_scale, 0);
					bg_transforms[i]->rotation = glm::quat(0.0f, 0.0f, 1.0f, 0.0f) * glm::quat(0.0f, 1.0f, 0.0f, 0.0f);
					bg_transforms[i]->scale = glm::vec3(5.2f * x_scale, 1, bgscale + 0.1f);
					tex_ind = *load_tex_wall_up;
					break;
				case 1:
					bg_transforms[i]->position = glm::vec3(0, 5.0f * y_scale, -2.0f * bgscale);
					bg_transforms[i]->rotation = glm::quat(0.0f, 0.0f, 1.0f, 0.0f) * glm::quat(0.0f, 1.0f, 0.0f, 0.0f);
					bg_transforms[i]->scale = glm::vec3(5.2f * x_scale, 1, bgscale);
					tex_ind = *load_tex_wall_up;
					break;
				case 2: // Down
					bg_transforms[i]->position = glm::vec3(0, -5.0f * y_scale, 0);
					bg_transforms[i]->scale = glm::vec3(5.2f * x_scale, 1, bgscale + 0.1f);
					tex_ind = *load_tex_wall_down;
					break;
				case 3:
					bg_transforms[i]->position = glm::vec3(0, -5.0f * y_scale, -2.0f * bgscale);
					bg_transforms[i]->scale = glm::vec3(5.2f * x_scale, 1, bgscale);
					tex_ind = *load_tex_wall_down;
					break;
				case 4: // Left
					bg_transforms[i]->position = glm::vec3(-5.0f * x_scale, 0, 0);
					bg_transforms[i]->scale = glm::vec3(1, 5.2f * y_scale, bgscale + 0.1f);
					tex_ind = *load_tex_wall_left;
					break;
				case 5:
					bg_transforms[i]->position = glm::vec3(-5.0f * x_scale, 0, -2.0f * bgscale);
					bg_transforms[i]->scale = glm::vec3(1, 5.2f * y_scale, bgscale);
					tex_ind = *load_tex_wall_left;
					break;
				case 6: // Right
					bg_transforms[i]->position = glm::vec3(5.0f * x_scale, 0, 0);
					bg_transforms[i]->rotation = glm::quat(0.0f, 0.0f, 1.0f, 0.0f) * glm::quat(0.0f, 1.0f, 0.0f, 0.0f);
					bg_transforms[i]->scale = glm::vec3(1, 5.2f * y_scale, bgscale + 0.1f);
					tex_ind = *load_tex_wall_right;
					break;
				case 7:
					bg_transforms[i]->position = glm::vec3(5.0f * x_scale, 0, -2.0f * bgscale);
					bg_transforms[i]->rotation = glm::quat(0.0f, 0.0f, 1.0f, 0.0f) * glm::quat(0.0f, 1.0f, 0.0f, 0.0f);
					bg_transforms[i]->scale = glm::vec3(1, 5.2f * y_scale, bgscale);
					tex_ind = *load_tex_wall_right;
					break;
				case 8: // Center front
					bg_transforms[i]->position = glm::vec3(0, 0, -bgscale);
					bg_transforms[i]->scale = glm::vec3(bgscale, bgscale, 1);
					tex_ind = *load_tex_clear;
					break;
				case 9: // Center back
					bg_transforms[i]->position = glm::vec3(0, 0, bgscale);
					bg_transforms[i]->scale = glm::vec3(5.0f * x_scale, 5.0f * y_scale, 1);
					tex_ind = *load_tex_game_over;
					break;
			}
			scene.drawables.emplace_back(bg_transforms[i]);
			Scene::Drawable &d_bg = scene.drawables.back();
			d_bg.pipeline = lit_color_texture_program_pipeline;
			d_bg.pipeline.vao = main_meshes_for_lit_color_texture_program;
			int ind = 0;
			if (i >= 2 * num_walls && i < 4 * num_walls) {
				ind = 1;
			} else if (i >= 4 * num_walls) {
				ind = 2;
			}
			d_bg.pipeline.type = backgrounds[ind].type;
			d_bg.pipeline.start = backgrounds[ind].start;
			d_bg.pipeline.count = backgrounds[ind].count;
			d_bg.pipeline.textures[0].texture = tex_ind;
		}

		// load actual audio files and create pairs
		song_list.emplace_back(std::make_pair("Tutorial", *load_song_tutorial));
		song_list.emplace_back(std::make_pair("The Beginning", *load_song_the_beginning));
		song_list.emplace_back(std::make_pair("Hellbound", *load_song_hellbound));
		song_list.emplace_back(std::make_pair("Halloween Madness", *load_song_halloween_madness));

		// ready to load main menu
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
		*** Each line must have its time begin and end be less than the time begins of all lines after it ***
*/
void PlayMode::read_notes(std::string song_name) {
	notes.clear();

	active_skin_idx = std::rand() % 2;

	// https://www.tutorialspoint.com/read-file-line-by-line-using-cplusplus
	std::fstream file;
	const char* delim = " ";
	file.open(data_path("beatmaps/" + song_name + ".txt"), std::ios::in);
	if (file.is_open()){
		std::string line;
		int note_idx_read = 0;
		while(getline(file, line)){
			std::vector<std::string> note_info;
			tokenize(line, delim, note_info);
			std::string note_type = note_info[0];
			int note_mesh_idx = stoi(note_info[1]);
			std::string dir = note_info[2];
			int idx = (int) (find(note_info.begin(), note_info.end(), "@") - note_info.begin());

			NoteInfo note;
			note.note_idx = note_idx_read;
			Mesh note_mesh = meshBuf->lookup(beatmap_skins[active_skin_idx].first + note_info[1]);
			note.min = note_mesh.min;
			note.max = note_mesh.max;
			note.dir = dir;

			note.scale = glm::vec3(0.2f, 0.2f, 0.2f);

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
					transform->position = glm::vec3((coords_begin.x + coords_end.x) / 2.0f, (coords_begin.y + coords_end.y) / 2.0f, init_note_depth - (time_end - time_begin) * note_speed / 2.0f);
					transform->scale = glm::vec3(0.0f, 0.0f, 0.0f); // all notes start from being invisible
					float angle = 0.0f;
					// if the xs are the same
					if(dir == "left") {
						angle = -atan2((coords_begin.y - coords_end.y), (time_end - time_begin) * note_speed);
						transform->rotation = normalize(glm::angleAxis(angle, glm::vec3(1.0f, 0.0f, 0.0f)) * glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
					}
					else if (dir == "right") {
						angle = -atan2((coords_begin.y - coords_end.y), (time_end - time_begin) * note_speed);
						transform->rotation = normalize(glm::angleAxis(angle, glm::vec3(1.0f, 0.0f, 0.0f)) * (glm::quat(0.0f, 0.0f, 1.0f, 0.0f) * glm::quat(0.0f, 1.0f, 0.0f, 0.0f)));
					}
					else if (dir == "up") {
						angle = atan2((coords_begin.x - coords_end.x), (time_end - time_begin) * note_speed);
						transform->rotation = normalize(glm::angleAxis(angle, glm::vec3(0.0f, 1.0f, 0.0f)) * glm::quat(0.7071f, 0.0f, 0.0f, -0.7071f));;
					}
					else {
						angle = atan2((coords_begin.x - coords_end.x), (time_end - time_begin) * note_speed);
						transform->rotation = normalize(glm::angleAxis(angle, glm::vec3(0.0f, 1.0f, 0.0f)) * glm::quat(0.7071f, 0.0f, 0.0f, 0.7071f));;
					}
					note.note_transforms.push_back(transform);
					note.hit_times.push_back(time_begin + real_song_offset);
					note.hit_times.push_back(time_end + real_song_offset);
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
				if(dir == "left") {
					transform->rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
				}
				else if (dir == "right") {
					transform->rotation = glm::quat(0.0f, 0.0f, 1.0f, 0.0f) * glm::quat(0.0f, 1.0f, 0.0f, 0.0f);
				}
				else if (dir == "up") {
					transform->rotation = glm::quat(0.7071f, 0.0f, 0.0f, -0.7071f);
				}
				else {
					transform->rotation = glm::quat(0.7071f, 0.0f, 0.0f, 0.7071f);
				}
				note.note_transforms.push_back(transform);
				note.hit_times.push_back(time + real_song_offset);
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

			note_idx_read += 1;
		}
		file.close();
	}
}

/*
	Helper function to create a scroll effect on the background
	Switches between two backgrounds on each direction to create an effect of it being infinite
*/
void PlayMode::update_bg(float elapsed) {

	for (size_t i = 0; i < bg_transforms.size() - 2; i++) {
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
void PlayMode::update_notes(float elapsed) {
	assert(game_state == PLAYING);

	auto current_time = std::chrono::high_resolution_clock::now();
	float music_time = std::chrono::duration<float>(current_time - music_start_time).count();

	if (!is_tutorial) {
		health = std::max(0.0f, health - elapsed / 50.0f);
		set_health_bar();
	}
	if (health < EPS_F) game_over(false);

	for (int i = note_start_idx; i < note_end_idx + 1; i++) {
		if (i >= (int)notes.size()) continue;
		auto &note = notes[i];
		// hold case - multiple note_transforms and hit_times
		if(note.noteType == NoteType::HOLD) {
			for (size_t j = 0; j < note.hit_times.size()-1; j++) {
				if (note.is_active) {
					if (music_time > note.hit_times[j+1] + valid_hit_time_delta) {
						// 'delete' the note
						if (!note.been_hit) hit_note(nullptr, -1);

						note.note_transforms[j]->scale = glm::vec3(0.0f, 0.0f, 0.0f);
						note_start_idx += 1;

						if (note_start_idx == (int)notes.size()) {
							song_cleared = true;
							song_clear_time = std::chrono::high_resolution_clock::now();
						}
					}
					else {
						// move the note
						float delta_time = music_time - (note.hit_times[j] - note_approach_time);
						note.note_transforms[j]->position.z = init_note_depth + note_speed * delta_time;
					}
				} else {
					if (!note.been_hit) {
						if (music_time >= note.hit_times[j] - note_approach_time) {
							// spawn the note
							note.is_active = true;
							note.note_transforms[j]->scale = glm::vec3(0.5f, 0.5f, note_speed * (note.hit_times[j+1] - note.hit_times[j]) / 4.0f);
							note.note_transforms[j]->position.z = init_note_depth - (note.hit_times[j+1] - note.hit_times[j]) / 2;
							note_end_idx += 1;
						}
					}
				}
			}
		}
		// single and burst case - only one note_transform and hit_time
		else {
			if (note.is_active) {
				if (note.been_hit){
					// after note has been hit, wait a bit before hiding "hit" mesh
					if (note.delete_time > 0.0f) {
						note.delete_time -= elapsed;
					} else {
						note.note_transforms[0]->scale = glm::vec3(0.0f, 0.0f, 0.0f);
					}
				} else {
					// move the note
					float delta_time = music_time - (note.hit_times[0] - note_approach_time);
					note.note_transforms[0]->position.z = init_note_depth + note_speed * delta_time;
				}

				if (music_time > note.hit_times[0] + valid_hit_time_delta) {
					// 'delete' the note
					if (!note.been_hit) hit_note(nullptr, -1);

					note.note_transforms[0]->scale = glm::vec3(0.0f, 0.0f, 0.0f);
					note_start_idx += 1;

					if (note_start_idx == (int)notes.size()) {
						song_cleared = true;
						song_clear_time = std::chrono::high_resolution_clock::now();
					}
				}
			} else {
				if (!note.been_hit) {
					if (music_time >= note.hit_times[0] - note_approach_time) {
						// spawn the note
						note.is_active = true;
						note.note_transforms[0]->scale = note.scale;
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
	
	t = tmin;
	return true;
}

/*
	Helper function to trace a ray from pos in dir direction against every visible note
		Transforms both pos and dir to note's local space in order to take care of OBB
*/
// currently using the same code three times, maybe think about what would actually be different between the three?
HitInfo PlayMode::trace_ray(glm::vec3 pos, glm::vec3 dir) {
	for (int i = note_start_idx; i < note_end_idx; i++) {
		NoteInfo &note = notes[i];

		if (note.note_transforms[0]->scale == glm::vec3()) continue;

		// single type
		if (note.noteType == NoteType::SINGLE) {
			if (note.been_hit) continue;
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
				HitInfo hits;
				hits.note = &notes[i];
				hits.time = t;
				return hits;
			}
		}
		// burst type
		else if(note.noteType == NoteType::BURST) {
			if (note.been_hit) continue;
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
				HitInfo hits;
				hits.note = &notes[i];
				hits.time = t;
				return hits;
			}
		}
		else {
			// incorrect note type breaks the game
			exit(1);
		}
	}

	return HitInfo();
}

void PlayMode::set_combo(int diff) {
	combo += diff;
	multiplier = combo / 25 + 1;
	if (combo > max_combo) max_combo = combo;
}

/*
	Update function to a note, score / combo and health
*/
void PlayMode::hit_note(NoteInfo* note, int hit_status) {

	if (hit_status == -1) {
		Sound::play(note_miss_sound);
		set_combo(-combo);
		if (is_tutorial) return;
		health = std::max(0.0f, health - 0.1f);
		set_health_bar();
		if (health < EPS_F) game_over(false);
		return;
	}

	auto curr_note = std::prev(scene.drawables.end(), notes.size());
	curr_note = std::next(curr_note, note->note_idx);

	// deactivate the note
	note->been_hit = true;

	switch (hit_status) {
		case 0:
			// bad hit, same as miss
			Sound::play(note_miss_sound);

			curr_note->pipeline.type = hit_miss.type;
			curr_note->pipeline.start = hit_miss.start;
			curr_note->pipeline.count = hit_miss.count;

			set_combo(-combo);
			if (!is_tutorial) {
				health = std::max(0.0f, health - 0.1f);
				if (health < EPS_F) game_over(false);
			}
			break;
		case 1:
			// good hit
			Sound::play(note_hit_sound);

			curr_note->pipeline.type = hit_good.type;
			curr_note->pipeline.start = hit_good.start;
			curr_note->pipeline.count = hit_good.count;

			score += 50 * multiplier;
			set_combo(1);
			health = std::min(max_health, health + 0.03f);
			break;
		case 2:
			// perfect hit
			Sound::play(note_hit_sound);

			curr_note->pipeline.type = hit_perfect.type;
			curr_note->pipeline.start = hit_perfect.start;
			curr_note->pipeline.count = hit_perfect.count;

			score += 100 * multiplier;
			set_combo(1);
			health = std::min(max_health, health + 0.03f);
			break;
		case 3:
			// wrong gun hit
			Sound::play(note_miss_sound);

			curr_note->pipeline.type = hit_miss.type;
			curr_note->pipeline.start = hit_miss.start;
			curr_note->pipeline.count = hit_miss.count;

			score += 10 * multiplier;
			set_combo(-combo);
			break;
		case 4:
			// hold begin and end
			Sound::play(note_hit_sound);
			score += 10 * multiplier;
			set_combo(1);
			health = std::min(max_health, health + 0.03f);
			break;
		case 5:
			// hold during success
			score += 1 * multiplier;
			set_combo(1);
			health = std::min(max_health, health + 0.0003f);
			break;
		case 6:
			// hold during fail
			set_combo(-combo);
			if (is_tutorial) {
				health = std::max(0.0f, health - 0.0003f);
			}
			break;
	}

	set_health_bar();
}

/*
	Function called whenever we click on the screen
		Checks if we hit any note, see if the hit is valid or not and then calls hit note based on that
*/
void PlayMode::check_hit(bool mouse_down=true) {
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
			if(fabs(music_time - hits.note->hit_times[0]) < valid_hit_time_delta && !holding && mouse_down) {
				// initial click
				hit_note(hits.note, 4);
			}
			else if(fabs(hits.note->hit_times[1] - music_time) < valid_hit_time_delta && !holding && !mouse_down) {
				// release near the end
				hit_note(hits.note, 4);
			}
			else if (holding) {
				// holding in between note
				// want to do linear interpolation between the hit_times depending on how fast the note is approaching
				glm::vec2 coord = get_coords(hits.note->dir, hits.note->coord_begin + (music_time - hits.note->hit_times[0] + real_song_offset) * (hits.note->coord_end - hits.note->coord_begin) / (hits.note->hit_times[1] - hits.note->hit_times[0]));
				
				glm::mat4 inverse = hits.note->note_transforms[0]->make_world_to_local();
				glm::vec3 start = glm::vec3(inverse * glm::vec4(camera->transform->position, 1.0f));
				glm::vec3 end = glm::vec3(inverse * glm::vec4(coord.x, coord.y, border_depth, 1.0f));
				float dist = glm::distance(start, end);
				end = glm::vec3(inverse * glm::vec4(coord.x, coord.y, 2.5f, 1.0f));
				if(gun_mode == 2 && dist - 0.5 < hits.time && hits.time < dist + 0.5) {
					hit_note(hits.note, 5);	
				}
				else {	
					hit_note(hits.note, 6);
				}
			}
		}
		else {
			if (holding || !mouse_down) return; // don't allow mouse LMB down hold cheese or lifting mouse button up

			// valid hit time for single and burst
			if(fabs(music_time - hits.note->hit_times[0]) < valid_hit_time_delta / 2.0f) {
				// good hit
				if ((gun_mode == 0 && hits.note->noteType == NoteType::SINGLE) || (gun_mode == 1 && hits.note->noteType == NoteType::BURST)){
					hit_note(hits.note, 2);
				} else {
					hit_note(hits.note, 3);
				}
			} else if (fabs(music_time - hits.note->hit_times[0]) < valid_hit_time_delta) {
				// ok hit
				if ((gun_mode == 0 && hits.note->noteType == NoteType::SINGLE) || (gun_mode == 1 && hits.note->noteType == NoteType::BURST)){
					hit_note(hits.note, 1);
				} else {
					hit_note(hits.note, 3);
				}
			}
			else {
				// bad hit
				hit_note(hits.note, 0);
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
	scene.drawables.erase(std::prev(scene.drawables.end(), notes.size()), scene.drawables.end());
	read_notes(song_list[chosen_song].first);
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

	bg_transforms[8]->position = glm::vec3(0.0f, 0.0f, -bgscale);
	bg_transforms[9]->position = glm::vec3(0.0f, 0.0f, bgscale);

	for (int i = 0; i < 3; i++) {
		gun_transforms[i]->scale = glm::vec3();
	}

	healthbar_transform->scale = glm::vec3();
	healthbarleft_transform->scale = glm::vec3();
	healthbarright_transform->scale = glm::vec3();
	border_transform->scale = glm::vec3();

	if (notes.size() >= 1) {
		reset_song();
		scene.drawables.erase(std::prev(scene.drawables.end(), notes.size()), scene.drawables.end());
	}
	reset_cam();

	// stop currently playing song
	if (active_song) active_song->stop();
	bg_loop = Sound::loop(*load_song_menu);
	bg_loop->set_volume(0.0f, 0.0f);
	bg_loop->set_volume(0.5f, 10.0f);
}

void PlayMode::set_health_bar() {
	if (health >= health_right_cutoff) {
		health_transform->scale.x = 1.0f;
		healthbarleft_transform->scale = glm::vec3(1.0f, 1.0f, 1.0f);
		healthbarright_transform->scale = glm::vec3(1.0f, 1.0f, 1.0f);
	} else if (health >= health_left_cutoff) {
		health_transform->scale.x = (health - health_left_cutoff) / (health_right_cutoff - health_left_cutoff);
		healthbarleft_transform->scale = glm::vec3(1.0f, 1.0f, 1.0f);
		healthbarright_transform->scale = glm::vec3(0.0f, 0.0f, 0.0f);
	} else {
		health_transform->scale.x = 0.0f;
		healthbarleft_transform->scale = glm::vec3(0.0f, 0.0f, 0.0f);
		healthbarright_transform->scale = glm::vec3(0.0f, 0.0f, 0.0f);
	}
}

/*
	Function that starts a song (and initializes all variables) when we choose to start a level
		start_song should only be called when going from MENU -> PLAYING or in restart_song
*/
void PlayMode::start_song(int idx, bool restart) {
	if (has_started) return;

	if (bg_loop) bg_loop->stop();

	song_cleared = false;
	holding = false;

	reset_cam();
	SDL_SetRelativeMouseMode(SDL_TRUE);

	note_start_idx = 0;
	note_end_idx = 0;
	score = 0;
	combo = 0;
	max_combo = 0;
	multiplier = 1;
	health = 0.7f;

	for (int i = 0; i < 3; i++) {
		gun_transforms[i]->scale = glm::vec3();
	}

	healthbar_transform->scale = healthbar_scale;
	healthbarleft_transform->scale = healthbar_LR_scale;
	healthbarright_transform->scale = healthbar_LR_scale;
	border_transform->scale = glm::vec3(x_scale, y_scale, z_scale);

	set_health_bar();

	change_gun(0, 0);

	has_started = true;
	game_state = PLAYING;
	chosen_song = idx;

	is_tutorial = false;
	if (song_list[chosen_song].first == "Tutorial") {
		is_tutorial = true;
	}

	music_start_time = std::chrono::high_resolution_clock::now();

	// choose the song based on index
	if (!restart) {
		read_notes(song_list[idx].first);
	}
	active_song = Sound::play(song_list[idx].second);
}

/*
	Function that restarts a song (and reinitializes all variables) when we choose to restart a level
		restart_song should only be called when going from PLAYING -> PAUSED -> select RESTART
*/
void PlayMode::restart_song() {
	bg_transforms[8]->position = glm::vec3(0.0f, 0.0f, -bgscale);
	bg_transforms[9]->position = glm::vec3(0.0f, 0.0f, bgscale);
	reset_song();
	has_started = false;
	start_song(chosen_song, true);
}

/*
	Function that pauses the game
		pause_song should only be called when going from PLAYING -> PAUSED
*/
void PlayMode::pause_song() {
	game_state = PAUSED;
	hovering_text = 0;
	music_pause_time = std::chrono::high_resolution_clock::now();
	active_song->pause(true);
}

/*
	Function that unpauses the game
		unpause_song should only be called when going from PLAYING -> PAUSED -> select RESUME
*/
void PlayMode::unpause_song() {
	game_state = PLAYING;
	SDL_SetRelativeMouseMode(SDL_TRUE);
	// TO CONSIDER : std::chrono's time may run slightly differently from SDL's audio timestamp. might want to compensate for this
	auto current_time = std::chrono::high_resolution_clock::now();
	music_start_time += current_time - music_pause_time;
	active_song->pause(false);
}

/*
	Function that ends the game and makes all events do nothing
		Should be called when either health reaches zero or if note_start_idx is equal to the end of the vector 
*/
void PlayMode::game_over(bool did_clear) {
	reset_cam();
	if (active_song) active_song->set_volume(0.0f, 3.0f);

	healthbar_transform->scale = glm::vec3();
	healthbarleft_transform->scale = glm::vec3();
	healthbarright_transform->scale = glm::vec3();
	border_transform->scale = glm::vec3();
	gun_transforms[gun_mode]->scale = glm::vec3();

	hovering_text = 0;
	if (did_clear) {
		game_state = SONGCLEAR;
		bg_transforms[8]->position = glm::vec3(0.0f, 0.0f, 2.0f);
	} else {
		game_state = GAMEOVER;
		bg_transforms[9]->position = glm::vec3(0.0f, 0.0f, 2.0f);
	}
	SDL_SetRelativeMouseMode(SDL_FALSE);
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
				return true;
			}
		}
	}
	else if (evt.type == SDL_MOUSEBUTTONDOWN) {
		if (game_state != PLAYING) return true;
		check_hit();
		holding = true;
	} else if (evt.type == SDL_MOUSEBUTTONUP) {
		if (game_state != PLAYING) return true;
		holding = false;
		check_hit(false);
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
		cam.azimuth = std::clamp(cam.azimuth - std::round(cam.azimuth), -0.05f, 0.05f);
		cam.azimuth *= 2.0f * 3.1415926f;

		cam.elevation /= 2.0f * 3.1415926f;
		cam.elevation = std::clamp(cam.elevation - std::round(cam.elevation), 0.2f, 0.3f);
		cam.elevation *= 2.0f * 3.1415926f;

		//update camera aspect ratio for drawable:
		camera->transform->rotation =
			normalize(glm::angleAxis(1280.0f / 720.0f * cam.azimuth, glm::vec3(0.0f, 1.0f, 0.0f))
			* glm::angleAxis(0.5f * 3.1415926f + -cam.elevation, glm::vec3(1.0f, 0.0f, 0.0f)));

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
		update_notes(elapsed);

		if (song_cleared && std::chrono::duration<float>(std::chrono::high_resolution_clock::now() - song_clear_time).count() > 3.0f) {
			game_over(true);
		}
	} else if (game_state == MENU) {
		update_bg(elapsed);
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
	// from forward lighting class demo
	//compute light uniforms:
	uint32_t lights = uint32_t(6);

	std::vector< int32_t > light_type; light_type.reserve(lights);
	std::vector< glm::vec3 > light_location; light_location.reserve(lights);
	std::vector< glm::vec3 > light_direction; light_direction.reserve(lights);
	std::vector< glm::vec3 > light_energy; light_energy.reserve(lights);
	std::vector< float > light_cutoff; light_cutoff.reserve(lights);

	light_location.emplace_back(camera->transform->position + glm::vec3(0.0f, 3.0f, 3.0f));
	light_direction.emplace_back(glm::vec3(0.0f, -0.1f, -1.0f));
	light_energy.emplace_back(glm::vec3(50.0f, 50.0f, 50.0f));
	light_type.emplace_back(0);
	light_cutoff.emplace_back(1.0f);

	light_location.emplace_back(camera->transform->position + glm::vec3(3.0f, 0.0f, -2.0f));
	light_direction.emplace_back(glm::vec3(1.0f, 0.0f, -1.0f));
	light_energy.emplace_back(glm::vec3(50.0f, 50.0f, 50.0f));
	light_type.emplace_back(0);
	light_cutoff.emplace_back(1.0f);
	light_location.emplace_back(camera->transform->position + glm::vec3(-3.0f, 0.0f, -2.0f));
	light_direction.emplace_back(glm::vec3(-1.0f, 0.0f, -1.0f));
	light_energy.emplace_back(glm::vec3(50.0f, 50.0f, 50.0f));
	light_type.emplace_back(0);
	light_cutoff.emplace_back(1.0f);
	light_location.emplace_back(camera->transform->position + glm::vec3(0.0f, 3.0f, -2.0f));
	light_direction.emplace_back(glm::vec3(0.0f, 1.0f, -1.0f));
	light_energy.emplace_back(glm::vec3(50.0f, 50.0f, 50.0f));
	light_type.emplace_back(0);
	light_cutoff.emplace_back(1.0f);
	light_location.emplace_back(camera->transform->position + glm::vec3(0.0f, -3.0f, -2.0f));
	light_direction.emplace_back(glm::vec3(0.0f, -1.0f, -1.0f));
	light_energy.emplace_back(glm::vec3(50.0f, 50.0f, 50.0f));
	light_type.emplace_back(0);
	light_cutoff.emplace_back(1.0f);

	light_location.emplace_back(camera->transform->position + glm::vec3(0.0f, 3.0f, 50.0f));
	light_direction.emplace_back(glm::vec3(0.0f, 0.0f, -1.0f));
	light_energy.emplace_back(glm::vec3(10000.0f, 10000.0f, 10000.0f));
	light_type.emplace_back(2);
	light_cutoff.emplace_back(1.0f);

	glUseProgram(lit_color_texture_program->program);
	glUniform1ui(lit_color_texture_program->LIGHTS_uint, lights);
	glUniform1iv(lit_color_texture_program->LIGHT_TYPE_int, lights, light_type.data());
	glUniform3fv(lit_color_texture_program->LIGHT_LOCATION_vec3, lights, glm::value_ptr(light_location[0]));
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, lights, glm::value_ptr(light_direction[0]));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, lights, glm::value_ptr(light_energy[0]));
	glUniform1fv(lit_color_texture_program->LIGHT_CUTOFF_float, lights, light_cutoff.data());
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
			lines.draw_text("Dungeon Beats",
				glm::vec3(-aspect + 0.3f + ofs, 0.7f, 0.0f),
				glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
				glm::u8vec4(0xff, 0xff, 0xff, 0x00));

			lines.draw_text("ESC to Exit",
				glm::vec3(aspect - 0.7f + ofs, -0.8f, 0.0f),
				glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
				glm::u8vec4(0xff, 0xff, 0xff, 0x00));

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
			glm::vec2 pos = glm::vec2(camera->transform->position.x, camera->transform->position.y);
			for (uint32_t a = 0; a < circle.size(); ++a) {
				for (float r = 0.02f; r >= 0.015f; r -= 0.001f) {
					lines.draw(
						glm::vec3(pos + r * circle[a], 0.0f),
						glm::vec3(pos + r * circle[(a+1)%circle.size()], 0.0f),
						glm::u8vec4(0xff, 0x00, 0x00, 0x00)
					);
				}
			}
			lines.draw_text(std::to_string(score),
				glm::vec3(aspect - 0.3f - ofs, 0.8f, 0.0f),
				glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
				glm::u8vec4(0xff, 0xff, 0xff, 0x00));

			lines.draw_text("x" + std::to_string(combo),
				glm::vec3(aspect - 0.3f - ofs, 0.0f, 0.0f),
				glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
				glm::u8vec4(0xff, 0xff, 0xff, 0x00));

			std::string gun_mode_text = "SINGLE";
			if (gun_mode == 1) gun_mode_text = "BURST";
			else if (gun_mode == 2) gun_mode_text = "HOLD";
			lines.draw_text(gun_mode_text,
				glm::vec3(-aspect + 0.3f + ofs, 0.8f, 0.0f),
				glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
				glm::u8vec4(0xff, 0xff, 0xff, 0x00));
			
			if (is_tutorial) {
				auto current_time = std::chrono::high_resolution_clock::now();
				float music_time = std::chrono::duration<float>(current_time - music_start_time).count();
				if (music_time < 16.0f) {
					lines.draw_text("Click enemies when they touch the square border",
						glm::vec3(-aspect + 0.3f + ofs, -0.8f, 0.0f),
						glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
						glm::u8vec4(0xff, 0xff, 0xff, 0x00));
				} else if (music_time < 24.0f) {
					lines.draw_text("Press +/- to increase/decrease mouse sensitivity",
						glm::vec3(-aspect + 0.3f + ofs, -0.8f, 0.0f),
						glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
						glm::u8vec4(0xff, 0xff, 0xff, 0x00));
				} else if (music_time < 36.0f) {
					lines.draw_text("Press X to change to BURST mode",
						glm::vec3(-aspect + 0.3f + ofs, -0.8f, 0.0f),
						glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
						glm::u8vec4(0xff, 0xff, 0xff, 0x00));
				} else if (music_time < 42.0f) {
					lines.draw_text("Click enemies when they touch the square border",
						glm::vec3(-aspect + 0.3f + ofs, -0.8f, 0.0f),
						glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
						glm::u8vec4(0xff, 0xff, 0xff, 0x00));
				} else if (music_time < 56.0f) {
					lines.draw_text("Press X to change to HOLD mode",
						glm::vec3(-aspect + 0.3f + ofs, -0.8f, 0.0f),
						glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
						glm::u8vec4(0xff, 0xff, 0xff, 0x00));
				} else if (music_time < 64.0f) {
					lines.draw_text("Hold click and follow enemies as they touch the square border",
						glm::vec3(-aspect + 0.3f + ofs, -0.8f, 0.0f),
						glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
						glm::u8vec4(0xff, 0xff, 0xff, 0x00));
				} else if (music_time < 72.0f) {
					lines.draw_text("Press X to change to SINGLE mode",
						glm::vec3(-aspect + 0.3f + ofs, -0.8f, 0.0f),
						glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
						glm::u8vec4(0xff, 0xff, 0xff, 0x00));
				} else if (music_time < 100.0f) {
					lines.draw_text("You can also press Z to go backwards",
						glm::vec3(-aspect + 0.3f + ofs, -0.8f, 0.0f),
						glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
						glm::u8vec4(0xff, 0xff, 0xff, 0x00));
				}
			}
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
			lines.draw_text(std::to_string(score),
				glm::vec3(aspect - 0.3f - ofs, 0.8f, 0.0f),
				glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
				glm::u8vec4(0xff, 0xff, 0xff, 0x00));

			lines.draw_text("x" + std::to_string(combo),
				glm::vec3(aspect - 0.3f - ofs, 0.0f, 0.0f),
				glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
				glm::u8vec4(0xff, 0xff, 0xff, 0x00));
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
			lines.draw_text("CLEARED " + song_list[chosen_song].first + "!",
				glm::vec3(-0.3f, 0.5f, 0.0f),
				glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
				glm::u8vec4(0xff, 0xff, 0xff, 0x00));

			lines.draw_text("SCORE: " + std::to_string(score),
				glm::vec3(-0.3f, 0.2f, 0.0f),
				glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
				glm::u8vec4(0xff, 0xff, 0xff, 0x00));

			lines.draw_text("MAX COMBO: " + std::to_string(max_combo),
				glm::vec3(-0.3f, 0.0f, 0.0f),
				glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
				glm::u8vec4(0xff, 0xff, 0xff, 0x00));
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
			lines.draw_text("FAILED " + song_list[chosen_song].first + "...",
				glm::vec3(-0.3f, 0.5f, 0.0f),
				glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
				glm::u8vec4(0xff, 0xff, 0xff, 0x00));
		}
	}
	GL_ERRORS();
}
