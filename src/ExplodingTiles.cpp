#include <string_view>
#include <iostream>
#include <optional>
#include <vector>
#include <memory>
#include <span>
#include <cmath>
#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>

#include "vectorops.hpp"
#include "coords.hpp"
#include "board.hpp"
#include "player.hpp"
#include "shapes.hpp"
#include "game.hpp"

std::string_view default_color_shader = R"(
#version 120
vec4 inv_sRGB(vec4 c) {
	vec3 rgb = c.rgb;
	vec3 low = rgb / 12.92f;
	vec3 high = pow((rgb + 0.055f)/1.055f, vec3(2.4f));
	bvec3 mask = lessThan(rgb,vec3(0.04045f));
	return vec4(mix(high,low,ivec3(mask)), c.a);
}

void main()
{
	// transform the vertex position
	gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;

	// transform the texture coordinates
	gl_TexCoord[0] = gl_TextureMatrix[0] * gl_MultiTexCoord0;

	// forward the vertex color
	gl_FrontColor = inv_sRGB(gl_Color);
}
)";

std::string_view board_shader = R"(
#version 130

const float edge_thickness = 0.04;
const vec3 edge_color = vec3(1);
const vec3 highlight_color = vec3(1,0,1);

uniform sampler2D board;
uniform int hex_size;
uniform ivec3 selected;
uniform float pulse_progress;

float min3(vec3 v) {
	return min(min(v.x,v.y),v.z);
}

float max3(vec3 v) {
	return max(max(v.x,v.y),v.z);
}

vec4 blend(vec4 under, vec4 over) {
	vec4 ret;
	ret.a = over.a + under.a*(1. - over.a);
	ret.rgb = over.a*over.rgb + under.a*under.rgb*(1.-over.a);
	return ret;
}

vec4 tile_color(ivec3 coords, ivec2 tile_data) {
	bool isUp = coords.x + coords.y + coords.z == (hex_size*3 - 1);
	bool isEdge = isUp ? any(equal(coords,ivec3(0))) : any(equal(coords,ivec3(hex_size*2-1)));
	int max = isEdge ? 1 : 2;
	if(tile_data.x == max) {
		return vec4(0.8,0.3,0.15,mix(0.3,1.,pulse_progress));
	}
	return vec4(0);
}

void main()
{
	vec3 min_bound = vec3(1);
	vec3 max_bound = vec3(hex_size*2+1);
	vec3 bound_edge = vec3(edge_thickness*2.);

	vec3 coordinates = (gl_Color.rgb) * float((hex_size+1) * 3);
	ivec3 coords = ivec3(floor(coordinates));
	
	if(all(greaterThan(coordinates,min_bound)) && all(lessThan(coordinates,max_bound))) {
		//Inner edge
		vec3 distance = min(fract(coordinates),ceil(coordinates)-coordinates);
		ivec3 current = coords - ivec3(1);
		if(selected == current) {
			float mix = smoothstep(0.,edge_thickness/2.,min3(distance));
			gl_FragColor.rgb = edge_color * (1.-mix) + mix*highlight_color;
			gl_FragColor.a = 1.-smoothstep(edge_thickness/2.,edge_thickness*1.5,min3(distance))*0.3;
		} else {
			gl_FragColor.rgb = edge_color;
			gl_FragColor.a = 1.-smoothstep(edge_thickness*0.8,edge_thickness,min3(distance));
		}
		ivec4 tile = ivec4(texelFetch(board,current.xy,0) * 255.);
		//grab the correct tile out
		ivec2 t = tile.rg;
		if(current.x + current.y + current.z == (hex_size*3 - 1)) {
			t = tile.ba;
		}
		vec4 color = tile_color(current,t);
		gl_FragColor = blend(color,gl_FragColor);
	} else if(all(greaterThan(coordinates,min_bound-bound_edge)) && all(lessThan(coordinates,max_bound+bound_edge))) {
		//Outer edge
		vec3 distance = max(min_bound - coordinates, coordinates - max_bound);
		gl_FragColor.rgb = edge_color;
		gl_FragColor.a = 1. - smoothstep(edge_thickness*1.5,edge_thickness*2.,max3(distance));
	} else {
		gl_FragColor = vec4(0);
	}
}
)";

class VisualBoard : public sf::Transformable, public sf::Drawable {
	sf::Vertex outer[3]; //for drawing
	sf::Vector2f inner[3]; //for coordinate calculations
	sf::Texture board_rep;
	sf::Image board_rep_img;
	sf::Clock start_time;
public:
	inline static sf::Shader* shader = nullptr;
	
	sf::Vector3i selected;
	int board_size;

	VisualBoard(float radius, int board_size) : board_size(board_size) {
		const float sqrt3 = std::sqrt(3.f);
		inner[0] = { 0,  -2 * radius };
		inner[1] = { radius * sqrt3, radius };
		inner[2] = { -radius * sqrt3, radius };

		const float outer_factor = (board_size + 1) / static_cast<float>(board_size); //Widen for outer edge
		
		outer[0] = sf::Vertex(inner[0]*outer_factor, sf::Color::Red);
		outer[1] = sf::Vertex(inner[1]*outer_factor, sf::Color::Green);
		outer[2] = sf::Vertex(inner[2]*outer_factor, sf::Color::Blue);

		//board_size is edge, each coordinate can vary between 0 and board_size*2.
		board_rep_img.create(board_size * 2, board_size * 2, sf::Color::Transparent);
		board_rep.setSmooth(false);
		board_rep.setRepeated(false);
		board_rep.setSrgb(false);
		board_rep.loadFromImage(board_rep_img);
	}

	//call whenever the board changes
	void update(const Board& b) {
		b.iterTiles([&](TriCoord c) {
			auto current = board_rep_img.getPixel(c.x, c.y);
			if (c.R) {
				current.r = b[c].num;
				current.g = b[c].player;
			}
			else {
				current.b = b[c].num;
				current.a = b[c].player;
			}
			board_rep_img.setPixel(c.x,c.y,current);
			return true;
		});

		board_rep.update(board_rep_img);
	}

	float getRadius() const {
		return (inner[1].y - inner[0].y) / 3;
	}

	float getTriRadius() const {
		return getRadius() / (board_size * 6 + 3);
	}

	TriCoord mouseToBoard(sf::Vector2f mouse) const {
		mouse = getInverseTransform().transformPoint(mouse);

		float length = inner[1].y - inner[0].y;
		float v1 = 1 - dot(mouse - inner[0], sf::Vector2f(0, 1)) / length;
		float v2 = 1 - dot(mouse - inner[1], inner[2] + (inner[0] - inner[2]) / 2.f - inner[1]) / (length * length);

		sf::Vector3i bary = sf::Vector3i(3.f * board_size * sf::Vector3f(v1, v2, 1 - v1 - v2));

		//ensure out of bounds coordinate when a coordinate < 0, converting to int != floor. Subtract one if a coordinate was below 0
		bary -= sf::Vector3i(v1 < 0, v2 < 0, v1 + v2 > 1);

		return TriCoord(bary, board_size);
	}

	sf::Vector2f baryToScreen(sf::Vector3f tri) const {
		return getTransform().transformPoint(tri.x * inner[0] + tri.y * inner[1] + tri.z * inner[2]);
	}

	void draw(sf::RenderTarget& target, sf::RenderStates states) const override {
		shader->setUniform("hex_size", board_size);
		shader->setUniform("selected", selected);
		float progress = inverseLerp(-1.f,1.f,std::sin(4*start_time.getElapsedTime().asSeconds()));
		shader->setUniform("pulse_progress", progress);
		target.draw(outer, 3, sf::PrimitiveType::Triangles, { sf::BlendAlpha, states.transform * getTransform(), &board_rep, shader });
	}
};

void draw_board(sf::RenderTarget& target, sf::RenderStates states, const VisualBoard& vis, const Board& b, std::span<const sf::CircleShape> players, float explosion_progress, bool draw_exploding_players = true) {
	target.draw(vis, states);
	
	sf::CircleShape explode{ vis.getTriRadius()*3 };
	explode.setFillColor(sf::Color::Yellow);
	explode.setOutlineColor(sf::Color::Red);
	explode.setOutlineThickness(-explode.getRadius() / 6);
	explode.setOrigin(explode.getRadius(), explode.getRadius());

	auto draw_tile = [&](TriCoord c, sf::RenderStates states) {
		auto s = b[c];
		if (s.num == 0) return;

		auto center = vis.baryToScreen(c.tri_center(b.size()));

		states.transform.translate(center);

		sf::CircleShape circle = players[s.player];
		const float size_diff = vis.getTriRadius() / circle.getRadius();
		circle.setScale(size_diff, size_diff);

		if (s.num > b.allowedPieces(c)) {
			auto explode_state = states;
			float scale = lerp(0.3f, 1.0f, explosion_progress);
			explode_state.transform.scale(scale, scale);
			target.draw(explode, explode_state);
			if(draw_exploding_players)
				for (const auto& n : c.neighbors()) {
					if (b.inBounds(n)) {
						auto move_target = vis.baryToScreen(n.tri_center(b.size())) - center;
						auto move_state = states;
						move_state.transform.translate(lerp(move_target / 3.f, move_target, explosion_progress));
						target.draw(circle, move_state);
					}
				}
			return;
		}

		sf::Vector2f offset{};

		if (s.num == 2 && c.R) {
			states.transform.translate(-circle.getOrigin() / 2.f);
		}

		target.draw(circle, states);

		if (s.num == 2) {
			states.transform.translate(circle.getOrigin() * 2.f / 3.f);
			target.draw(circle, states);
		}
	};

	b.iterTiles([&](auto c) {draw_tile(c, states); return true; });
}

class ScoreBar : public sf::Drawable {
	std::vector<std::pair<sf::Color,float>> players;
	sf::FloatRect bar;
public:
	ScoreBar(sf::FloatRect loc) : bar(loc) {}

	void addPlayer(sf::Color c) {
		players.emplace_back(c, 0.f);
	}

	void update(const Board& b) {
		auto player_counts = b.playerTotals();
		for (size_t p = 0; p < player_counts.size(); ++p) {
			players[p].second = lerp<float>(players[p].second, static_cast<float>(player_counts[p]), 0.3f);
		}
	}

	void reset() {
		for (auto& p : players) p.second = 0.f;
	}

	void draw(sf::RenderTarget& target, sf::RenderStates states) const override {
		sf::RectangleShape s{ {bar.width,bar.height} };
		s.setPosition(bar.left, bar.top);
		s.setFillColor(sf::Color::Black);
		target.draw(s,states);

		const auto total = std::accumulate(players.begin(), players.end(), 0.f, [](float acc, auto&& p) {return acc + p.second; });
		if (total > 0) {
			s.setSize({ 0,bar.height });
			for (auto& [color,size] : players) {
				float new_s = bar.width * size / total;
				s.setPosition(s.getPosition().x + s.getSize().x, bar.top);
				s.setSize({ new_s, bar.height });
				s.setFillColor(color);
				target.draw(s,states);
			}
		}
	}
};

constexpr float explosion_length = 0.5f;

namespace state_transitions {
	struct PlayerInfo {
		int shape_points;
		sf::Color color;
		PlayerType playerBehavior;
	};
	struct StartGame {
		std::vector<PlayerInfo> players;
		int board_size = 3;
	};
	struct ReturnToMain {};
	struct OpenPlayerSelect {};
	struct OpenTutorial {};
	using StateChangeEvent = std::variant<std::monostate, OpenPlayerSelect, StartGame, ReturnToMain, OpenTutorial>;
}

class State : public sf::Drawable {
public:
	virtual void update() = 0;
	virtual void mouseMove(sf::Vector2f mouse) = 0;
	virtual state_transitions::StateChangeEvent onClick(sf::Vector2f mouse) = 0;
};

class GameState : public State {
	BoardWithPlayers board;
	VisualBoard visual_board;

	sf::Clock explode_timer;

	sf::VertexArray reset_arrow;

	sf::Vector2f show_current_player;
	std::vector<sf::CircleShape> players;

	ScoreBar bar;
	CrossShape exit;

	void addPlayer(int polygon_n, sf::Color color, std::unique_ptr<Player> controller) {
		board.addPlayer(std::move(controller));
		bar.addPlayer(color);
		players.push_back(playerShape(polygon_n, color, visual_board.getTriRadius()));
	}

public:
	GameState(sf::Vector2f center, float radius, const state_transitions::StartGame& game_info) 
		: board(game_info.board_size), visual_board(radius * .9f, game_info.board_size), bar({ center - sf::Vector2f(radius, radius), sf::Vector2f(2 * radius, radius * 0.1f) }), exit(sf::Color::Red,40) {
		
		exit.setPosition(60, 60);
		exit.setRotation(45);

		center.y += radius * 0.2f;
		radius *= .9f;

		visual_board.setPosition(center);

		sf::Transform rot = sf::Transform().rotate(-60);

		sf::Vector2f extra_offset{ 0, radius*1.4f };

		extra_offset = rot.transformPoint(extra_offset);

		show_current_player = center - extra_offset;

		extra_offset = rot.transformPoint(extra_offset);

		reset_arrow = circArrow(center - extra_offset, sf::Color::White, 15, 24, 5);

		for (auto& [num, color, behavior] : game_info.players) {
			addPlayer(num, color, toPlayer(behavior));
		}
	}

	void mouseMove(sf::Vector2f mouse) override {
		if (exit.getBounds().contains(mouse)) {
			exit.setColor(sf::Color::Yellow);
		}
		else {
			exit.setColor(sf::Color::Red);
		}
		board.getCurrentPlayer().onInput(input_events::MouseMove{ visual_board.mouseToBoard(mouse) });
	}

	state_transitions::StateChangeEvent onClick(sf::Vector2f mouse) override {
		if (reset_arrow.getBounds().contains(mouse)) {
			board.reset();
			bar.reset();
			visual_board.update(board.getBoard());
		}
		else if (exit.getBounds().contains(mouse)) {
			return state_transitions::ReturnToMain{};
		}
		else {
			board.getCurrentPlayer().onInput(input_events::MouseClick{ visual_board.mouseToBoard(mouse) });
		}
		return {}; //no transitions yet
	}

	void update() override {
		if (not board.getWinner() && (not board.getBoard().needsUpdate() || explode_timer.getElapsedTime().asSeconds() > explosion_length)) {
			if (board.update()) {
				visual_board.update(board.getBoard());
			}
			explode_timer.restart();
		}
		visual_board.selected = board.getCurrentPlayer().selected().bary(board.getBoard().size());
		bar.update(board.getBoard());
	}

	void draw(sf::RenderTarget& target, sf::RenderStates states) const override {
		if (auto player = board.getWinner(); player) {
			target.draw(visual_board, states);
			
			auto current = players[*player];
			current.setRadius(visual_board.getRadius() / 8);
			current.setOrigin(current.getRadius(), current.getRadius());
			current.setPosition(show_current_player);
			target.draw(current, states);
			
			//Show winner
			current.setRadius(visual_board.getRadius());
			current.setOrigin(current.getRadius(), current.getRadius());
			current.setPosition(visual_board.getPosition());
			current.setOutlineThickness(current.getRadius() / 20);
			target.draw(current, states);
		}
		else {
			const float explosion_progress = explode_timer.getElapsedTime().asSeconds() / explosion_length;
			draw_board(target, states, visual_board, board.getBoard(), players, explosion_progress);
			
			auto current = players[board.getCurrentPlayerNum()];
			current.setRadius(visual_board.getRadius() / 8);
			current.setPosition(show_current_player);
			target.draw(current, states);
		}

		target.draw(reset_arrow, states);
		target.draw(bar, states);
		target.draw(exit, states);
	}
};

class Logo : public sf::Transformable, public sf::Drawable {
	VisualBoard board;
	Board b;
	std::array<sf::CircleShape, 2> players;
	sf::VertexArray trail;

	void draw_trail(sf::RenderTarget& target, sf::RenderStates states, sf::Vector2f start, float length, float angle, sf::CircleShape p) const {
		length /= 2;
		sf::Transform draw;
		draw.translate(start).scale(length,length).rotate(angle).translate(1,0);

		auto trail_state = states;
		trail_state.transform *= draw;
		target.draw(trail, trail_state);

		p.setRadius(p.getRadius() * 5);
		p.setOutlineThickness(2);
		p.setRotation(angle / 2);
		p.setOrigin(p.getRadius(), p.getRadius());
		p.setPosition(draw.transformPoint(1, 0));
		target.draw(p, states);
	}

public:
	Logo(float size) : board(size/2, 2), b(2), players{ {playerShape(3,sf::Color::Red,board.getTriRadius()),playerShape(5,sf::Color::Yellow,board.getTriRadius())} } {
		//Exploding tile
		b.incTile({ 2,1,true }, 0);
		b.incTile({ 2,1,true }, 0);
		b.incTile({ 2,1,true }, 0);

		//semi-randomly filled tiles
		b.incTile({ 3,2,false }, 0);
		b.incTile({ 2,2,true }, 1);
		b.incTile({ 2,2,true }, 1);
		b.incTile({ 2,0,true }, 1);
		b.incTile({ 1,1,false }, 0);
		b.incTile({ 0,3,true }, 0);

		sf::Color transparent = sf::Color::White;
		transparent.a = 0;
		sf::Color halftrans = transparent;
		halftrans.a = 127;

		trail.setPrimitiveType(sf::PrimitiveType::TriangleFan);
		trail.append({ {0,0}, halftrans });
		trail.append({ { 1,0 }, sf::Color::White });
		trail.append({ { 1,0.25f }, transparent });
		trail.append({ { -1,0 }, transparent });
		trail.append({ { 1,-0.25f }, transparent });
		trail.append({ { 1,0 }, sf::Color::White });
		board.update(b);
	}

	void draw(sf::RenderTarget& target, sf::RenderStates states) const override {
		states.transform *= getTransform();
		draw_board(target, states, board, b, players, 3, false); //huge explosion :D
		draw_trail(target, states, board.baryToScreen(TriCoord{ 2,1,true }.tri_center(board.board_size)), board.getRadius() * 1.5f, 150, players[0]);
		draw_trail(target, states, board.baryToScreen(TriCoord{ 2,1,true }.tri_center(board.board_size)), board.getRadius() * 1.5f, 30, players[0]);
		draw_trail(target, states, board.baryToScreen(TriCoord{ 2,1,true }.tri_center(board.board_size)), board.getRadius() * 1.5f, 90, players[1]);
	}
};

class MainMenu : public State {
	sf::CircleShape play_button;
	QuestionMark tutorial;
	Logo logo;
public:
	MainMenu(sf::Vector2f dims) : play_button(dims.x / 20, 3), tutorial(dims.x/15), logo(dims.y / 2.4f) {
		play_button.setFillColor(sf::Color::Yellow);
		play_button.setRotation(-30);
		play_button.setOrigin(sf::Vector2f(play_button.getRadius(), play_button.getRadius()));
		play_button.setPosition(dims.x / 3, dims.y * 4/5);
		tutorial.setOrigin(tutorial.getBounds().width / 2, tutorial.getBounds().height / 2);
		tutorial.setPosition(dims.x * 2 / 3, play_button.getPosition().y);

		logo.setPosition(dims.x/2,dims.y / 4 + 10);
	}

	void mouseMove(sf::Vector2f mouse) override {
	}

	state_transitions::StateChangeEvent onClick(sf::Vector2f mouse) override {
		if (play_button.getGlobalBounds().contains(mouse)) {
			return state_transitions::OpenPlayerSelect{};
		}
		else if (tutorial.getBounds().contains(mouse)) {
			return state_transitions::OpenTutorial{};
		}
		return {};
	}

	void update() override {

	}

	void draw(sf::RenderTarget& target, sf::RenderStates states) const override {
		target.draw(logo,states);
		target.draw(play_button,states);
		target.draw(tutorial, states);
	}
};

class AISelector : public sf::Transformable, public sf::Drawable {
	PlayerType selected = PlayerType::AIRando;
	std::array<StarShape, 3> stars;
	AIPlayerShape shape;

	StarShape make_star(float size) {
		StarShape ret(size / 2, size, 5);
		ret.setOutlineColor(sf::Color::Black);
		ret.setOutlineThickness(1);
		ret.setFillColor(sf::Color::Transparent);
		return ret;
	}

	void updateStars() {
		for (auto& s : stars) {
			s.setFillColor(sf::Color::Transparent);
		}
		auto fill_color = sf::Color::Yellow;
		switch (selected) {
		case PlayerType::AISmart:
			stars[0].setFillColor(fill_color);
			[[fallthrough]];
		case PlayerType::AIGreedy:
			stars[1].setFillColor(fill_color);
			[[fallthrough]];
		case PlayerType::AIRando:
			stars[2].setFillColor(fill_color);
			break;
		default:
			break;
		}
	}

public:
	AISelector(float size) : shape(size) {
		std::ranges::fill(stars, make_star(size / 8));
		auto loc = shape.getBounds();
		for (auto& star : stars) {
			star.setPosition(loc.width + size / 8 + size / 10, loc.top + size / 8);
			loc.top += size / 4 + 5;
		}
		updateStars();
	}

	sf::FloatRect getBounds() const {
		auto shaperect = shape.getBounds();
		auto starrect = stars[0].getGlobalBounds();
		return getTransform().transformRect(sf::FloatRect(shaperect.left,shaperect.top,(starrect.left+starrect.width)-shaperect.left, shaperect.height));
	}

	PlayerType onMouseClick(sf::Vector2f mouse) {
		mouse = getInverseTransform().transformPoint(mouse);
		auto clicked = std::ranges::find_if(stars, [mouse](auto& s) {return s.getGlobalBounds().contains(mouse); });
		if (clicked != stars.end()) {
			selected = static_cast<PlayerType>(3 - (clicked - stars.begin()));
			updateStars();
		}
		return selected;
	}

	void draw(sf::RenderTarget& target, sf::RenderStates states) const override {
		states.transform *= getTransform();
		target.draw(shape, states);
		for (auto& s : stars) {
			target.draw(s, states);
		}
	}
};

template<class ShapeType>
class RectSelector : public sf::Drawable {
	std::vector<ShapeType> shapes;
	size_t selected = 0;
	sf::FloatRect bounds;

	void select(sf::Shape& s) {
		s.setOutlineColor(sf::Color::White);
		s.setOutlineThickness(2);
	}

	void deselect(sf::Shape& s) {
		s.setOutlineColor(sf::Color::Black);
		s.setOutlineThickness(1);
	}

public:
	RectSelector() = default;

	constexpr static int padding = 10;
	static float shape_width(int total, float width) {
		return (width - padding * (total + 1)) / total;
	}


	RectSelector(std::vector<ShapeType>&& s, float width, size_t selected) : shapes(std::move(s)), selected(selected) {
		const float individual_width = shape_width(shapes.size(), width);

		bounds = sf::FloatRect(0, 0, width, individual_width);

		size_t i = 0;
		for (sf::Shape& shape : shapes) {
			shape.setPosition(padding + (individual_width + padding) * i + shape.getOrigin().x, shape.getOrigin().y);
			if (i == selected) {
				select(shape);
			}
			else {
				deselect(shape);
			}
			++i;
		}
	}

	ShapeType* onClick(sf::Vector2f mouse) {
		for (auto& s : shapes) {
			if (s.getGlobalBounds().contains(mouse)) {
				deselect(shapes[selected]);
				selected = &s - &shapes.front();
				select(s);
				return &s;
			}
		}
		return nullptr;
	}

	sf::FloatRect getBounds() const {
		return bounds;
	}

	void draw(sf::RenderTarget& target, sf::RenderStates states) const override {
		for (auto& s : shapes) {
			target.draw(s, states);
		}
	}
};

class ShapeSelector : public sf::Transformable, public sf::Drawable {
	RectSelector<sf::CircleShape> selector;

public:
	ShapeSelector(float width, int current_num) {
		std::vector<sf::CircleShape> shapes;
		const float individual_width = selector.shape_width(4, width);
		for (int i = 3; i <= 6; ++i) {
			shapes.push_back(playerShape(i, sf::Color::Transparent, individual_width / 2));
		}
		selector = { std::move(shapes), width, static_cast<size_t>(current_num - 3)};
	}

	sf::FloatRect getBounds() const {
		return getTransform().transformRect(selector.getBounds());
	}

	//returns number of shape clicked on, -1 otherwise
	int onClick(sf::Vector2f mouse) {
		mouse = getInverseTransform().transformPoint(mouse);
		if (auto* s = selector.onClick(mouse); s) {
			return s->getPointCount();
		}
		return -1;
	}

	void draw(sf::RenderTarget& target, sf::RenderStates states) const override {
		states.transform *= getTransform();
		target.draw(selector, states);
	}
};

constexpr std::array<uint32_t,4> colors{ 0xA41A1CFF, 0xDEDE00FF, 0xFF7F00FF, 0xA65628FF};

class ColorSelector : public sf::Transformable, public sf::Drawable {
	RectSelector<sf::RectangleShape> selector;

public:
	ColorSelector(float width, sf::Color initial) {
		const float individual_width = selector.shape_width(colors.size(),width);

		std::vector<sf::RectangleShape> shapes;

		for (auto c : colors) {
			shapes.push_back(sf::RectangleShape(sf::Vector2f(individual_width,individual_width)));
			shapes.back().setFillColor(sf::Color(c));
		}

		size_t selected = std::ranges::find(colors, initial.toInteger()) - colors.begin();
		selector = { std::move(shapes), width, selected};
	}

	sf::FloatRect getBounds() const {
		return getTransform().transformRect(selector.getBounds());
	}

	//returns number of shape clicked on, -1 otherwise
	std::optional<sf::Color> onClick(sf::Vector2f mouse) {
		mouse = getInverseTransform().transformPoint(mouse);
		if (auto* s = selector.onClick(mouse); s) {
			return s->getFillColor();
		}
		return {};
	}

	void draw(sf::RenderTarget& target, sf::RenderStates states) const override {
		states.transform *= getTransform();
		target.draw(selector, states);
	}
};

class PlayerSelector : public sf::Transformable, public sf::Drawable {
	sf::RectangleShape outline;
	HumanPlayer human_shape;
	AISelector AI_shape;
	sf::CircleShape player_shape;
	ShapeSelector shape_selector;
	ColorSelector color_selector;
	CrossShape remove;
	state_transitions::PlayerInfo player;
	sf::RectangleShape selector;

	void refresh() {
		player_shape = playerShape(player.shape_points, player.color, outline.getSize().x / 4);
		player_shape.setPosition(outline.getSize().x / 2, player_shape.getRadius() + 10);

		shape_selector.setPosition(0, player_shape.getPosition().y + player_shape.getRadius() + 5);
		auto shape_pos = shape_selector.getBounds();
		color_selector.setPosition(0, shape_pos.top + shape_pos.height + 5);
		
		sf::FloatRect selector_pos = (player.playerBehavior == PlayerType::Mouse) ? human_shape.getBounds() : AI_shape.getBounds();
		selector.setPosition(selector_pos.left - 3,selector_pos.top - 3);
		selector.setSize(sf::Vector2f(selector_pos.width + 6, selector_pos.height + 6));
	}

public:
	PlayerSelector(sf::Vector2f size, state_transitions::PlayerInfo info) 
		: outline(size), human_shape(size.x / 3), AI_shape(size.x / 3), 
		  shape_selector(size.x, info.shape_points), color_selector(size.x,info.color), 
		  remove(sf::Color::Red,size.x/5), player(info) {

		outline.setOutlineColor(sf::Color::Cyan);
		outline.setFillColor(sf::Color::Transparent);
		outline.setOutlineThickness(2.f);

		human_shape.setPosition(0, size.y - human_shape.getBounds().height - 10);
		AI_shape.setPosition(sf::Vector2f(size.x - AI_shape.getBounds().width - 10, human_shape.getPosition().y));
		selector.setFillColor(sf::Color::Transparent);
		selector.setOutlineColor(sf::Color::White);
		selector.setOutlineThickness(2.f);

		remove.setPosition(size.x, 0);
		remove.setRotation(45);

		refresh();
	}

	//returns whether the remove button was pressed
	bool onMouseClick(sf::Vector2f mouse) {
		mouse = getInverseTransform().transformPoint(mouse);
		if (human_shape.getBounds().contains(mouse)) {
			player.playerBehavior = PlayerType::Mouse;
			refresh();
		}
		else if (AI_shape.getBounds().contains(mouse)) {
			player.playerBehavior = AI_shape.onMouseClick(mouse);
			refresh();
		}
		else if (shape_selector.getBounds().contains(mouse)) {
			if (int n = shape_selector.onClick(mouse); n != -1) {
				player.shape_points = n;
				refresh();
			}
		}
		else if (color_selector.getBounds().contains(mouse)) {
			if (auto c = color_selector.onClick(mouse); c) {
				player.color = *c;
				refresh();
			}
		}
		else if (remove.getBounds().contains(mouse)) {
			return true;
		}
		return false;
	}

	operator state_transitions::PlayerInfo() const {
		return player;
	}

	sf::FloatRect getBounds() const {
		const float outside_outline = remove.getBounds().height / 2;
		return getTransform().transformRect(sf::FloatRect(0,-outside_outline/2,outline.getSize().x+outside_outline/2, outline.getSize().y+outside_outline));
	}

	void draw(sf::RenderTarget& target, sf::RenderStates states) const override {
		states.transform.combine(getTransform());
		target.draw(outline, states);
		target.draw(selector, states);
		target.draw(human_shape, states);
		target.draw(AI_shape, states);
		target.draw(player_shape,states);
		target.draw(shape_selector, states);
		target.draw(color_selector, states);
		target.draw(remove, states);
	}
};

class PlayerSelect : public State {
	CrossShape add_player;
	sf::CircleShape play_button;
	std::vector<PlayerSelector> players;
	sf::Vector2f dims;

	VisualBoard board;
	sf::CircleShape increase_board, decrease_board;
	
	static constexpr size_t max_players = 5;

	sf::Vector2f player_select_size() const {
		return { dims.x / 6,dims.x / 4 };
	}

	void updateLayout() {
		const auto size = player_select_size();
		const float padding = 20;

		const float y_center = dims.y - size.y/2 - padding;

		float left = dims.x / 2 - (size.x+padding) * players.size() / 2 + padding/2;
		for (auto& p : players) {
			p.setPosition({ left, y_center - size.y/2 });
			left += padding + size.x;
		}
		add_player.setPosition({ left + add_player.getBounds().width/2, y_center });
	}

	void nextPlayer() {
		switch (players.size()) {
		case 0:
			players.push_back(PlayerSelector{ player_select_size(), {3, sf::Color(colors[0]), PlayerType::Mouse} });
			break;
		default:
			players.push_back(PlayerSelector{ player_select_size(), { 5, sf::Color(colors[1]), PlayerType::AIRando } });
			break;
		}
		updateLayout();
	}

public:
	PlayerSelect(sf::Vector2f dims) : add_player(sf::Color::Green,dims.x/20), play_button(dims.x / 20, 3), dims(dims), 
		board(dims.y/5,3), increase_board(dims.x/30,3) {
		
		play_button.setFillColor(sf::Color::Yellow);
		play_button.setRotation(-30);
		play_button.setOrigin(sf::Vector2f(play_button.getRadius(),play_button.getRadius()));
		play_button.setPosition(dims.x - play_button.getRadius() - 20, dims.y / 2);

		board.setPosition(sf::Vector2f(dims.x / 2, dims.y / 4 + 20));

		increase_board.setFillColor(sf::Color::Green);
		increase_board.setOutlineColor(sf::Color::Black);
		increase_board.setOutlineThickness(1);
		increase_board.setScale(1, 1.2f);
		increase_board.setOrigin(increase_board.getRadius(), increase_board.getRadius());

		decrease_board = increase_board;
		decrease_board.rotate(180);
		
		const float dist = dims.y / 5 + 2*increase_board.getRadius();

		decrease_board.setPosition(board.getPosition() + sf::Vector2f(dist, decrease_board.getRadius()));
		increase_board.setPosition(board.getPosition() + sf::Vector2f(dist, -increase_board.getRadius()));

		nextPlayer();
		nextPlayer();
	}

	void mouseMove(sf::Vector2f mouse) override {
	}

	state_transitions::StateChangeEvent onClick(sf::Vector2f mouse) override {
		if (play_button.getGlobalBounds().contains(mouse)) {
			state_transitions::StartGame ret;
			std::copy(players.begin(), players.end(), std::back_inserter(ret.players));
			ret.board_size = board.board_size;
			return ret;
		} else if (players.size() < max_players && add_player.getBounds().contains(mouse)) {
			nextPlayer();
		}
		else if (increase_board.getGlobalBounds().contains(mouse)) {
			board.board_size++;
		}
		else if (decrease_board.getGlobalBounds().contains(mouse)) {
			board.board_size = std::max(1, board.board_size - 1);
		} else {
			for (auto& p : players) {
				if (p.getBounds().contains(mouse)) {
					if (p.onMouseClick(mouse)) {
						auto pos = &p - &players.front();
						players.erase(players.begin() + pos);
						updateLayout();
					}
					break;
				}
			}
		}
		return {};
	}

	void update() override {

	}

	void draw(sf::RenderTarget& target, sf::RenderStates states) const override {
		if(players.size() < max_players) 
			target.draw(add_player,states);
		target.draw(play_button,states);
		target.draw(board, states);
		target.draw(decrease_board, states);
		target.draw(increase_board, states);
		for (auto& p : players) {
			target.draw(p,states);
		}
	}

};

struct Move {
	TriCoord coord;
	int player;
};

class BoardAnimation : public sf::Transformable, public sf::Drawable {
	Board current;
	VisualBoard visual_board;

	sf::Clock timer;

	std::span<const Move> setup;
	std::span<const Move> moves;

	decltype(moves.begin()) current_move;

	std::span<const sf::CircleShape> player_shapes;

	static constexpr float time_between_moves = 0.8f;
	static constexpr float time_for_mouse = 0.6f;
	static constexpr float click_duration = 0.1f;

	sf::CircleShape mouse;
	sf::Vector2f mouse_diff;
	bool click = false;

	void setMouseFromSelected() {
		mouse.setPosition(visual_board.baryToScreen(TriCoord(visual_board.selected, visual_board.board_size).tri_center(visual_board.board_size)));
		mouse_diff = {};
	}

public:
	BoardAnimation(std::span<const sf::CircleShape> players, float radius, int board_size, std::span<const Move> setup, std::span<const Move> moves)
		: current(board_size), visual_board(radius, board_size), setup(setup), moves(moves), player_shapes(players), mouse(radius / 30) {
		mouse.setFillColor(sf::Color::Red);
		mouse.setOutlineColor(sf::Color::White);
		mouse.setOrigin(mouse.getRadius(), mouse.getRadius());
		mouse.setOutlineThickness(2.5f);
		reset();
	}

	void reset() {
		current = { current.size() };
		for (auto& m : setup) {
			current.incTile(m.coord, m.player);
		}
		visual_board.update(current);
		current_move = moves.begin();
		
		if (setup.size() > 0) visual_board.selected = setup.back().coord.bary(visual_board.board_size);
		else visual_board.selected = {};
		setMouseFromSelected();
		timer.restart();
	}

	void update() {
		const float elapsed = timer.getElapsedTime().asSeconds();
		sf::Vector2f current_mouse_pos = mouse.getPosition() + elapsed / time_for_mouse * mouse_diff;
		visual_board.selected = visual_board.mouseToBoard(current_mouse_pos).bary(visual_board.board_size);
		if (click && elapsed > click_duration) {
			click = false;
			mouse.setScale(1, 1);
		}
		if (current.needsUpdate()) {
			if (elapsed > explosion_length) {
				current.update_step();
				visual_board.update(current);
				timer.restart();
			}
		}
		else if (elapsed > time_between_moves) {
			if (current_move == moves.end()) {
				reset();
			}
			else {
				Move m = *current_move;
				if (visual_board.selected != m.coord.bary(visual_board.board_size)) {
					mouse_diff = visual_board.baryToScreen(m.coord.tri_center(visual_board.board_size)) - mouse.getPosition();
				}
				else {
					current.incTile(m.coord, m.player);
					visual_board.update(current);
					click = true;
					mouse.setScale(0.4f, 0.4f);
					++current_move;
				}
			}
			timer.restart();
		}
		else if (elapsed > time_for_mouse) {
			setMouseFromSelected();
		}
	}

	void draw(sf::RenderTarget& target, sf::RenderStates states) const override {
		states.transform *= getTransform();
		const float elapsed = timer.getElapsedTime().asSeconds();
		draw_board(target, states, visual_board, current, player_shapes, elapsed / explosion_length);
		states.transform *= sf::Transform().translate(elapsed / time_for_mouse * mouse_diff);
		target.draw(mouse, states);
	}
};

namespace tutorial_detail {
	static constexpr std::array<Move, 0> empty{};
}

static constexpr auto tut1_setup = tutorial_detail::empty;
static constexpr auto tut1_moves = std::array{
	Move{ TriCoord(0,1,true),0 },
	Move{ TriCoord(0,0,true),1 },
	Move{ TriCoord(0,1,true),0 }
};

static constexpr auto tut2_setup = std::array{
	Move{TriCoord(1,1,false),0 },
	Move{TriCoord(0,2,false),1 },
	Move{TriCoord(0,2,true),0}, //taken over by yellow
	Move{TriCoord(0,0,true),0} //start out-of-bounds
};
static constexpr auto tut2_moves = std::array{
	Move{TriCoord(1,1,false),0},
	Move{TriCoord(0,2,false),1},
	Move{TriCoord(1,1,false),0}
};

class TutorialState : public State {
	CrossShape exit;
	std::array<sf::CircleShape,2> players = { playerShape(3,sf::Color(colors[0])), playerShape(5,sf::Color(colors[1])) };
	sf::CircleShape prev_tut,next_tut;
	std::vector<BoardAnimation> anims;
	std::size_t current = 0;
public:
	TutorialState(sf::Vector2f dims) : exit(sf::Color::Red, 40), next_tut(dims.y/15,3) {
		exit.setPosition(60, 60);
		exit.setRotation(45);

		next_tut.setFillColor(sf::Color::Yellow);
		next_tut.setOrigin(next_tut.getRadius(), next_tut.getRadius()*9/4);
		next_tut.setScale(1, 1.2f);
		next_tut.setPosition(dims.x/2,dims.y - next_tut.getRadius()*2);
		next_tut.rotate(90);

		prev_tut = next_tut;
		prev_tut.rotate(180);

		auto add_anim = [&](int board_size, std::span<const Move> setup, std::span<const Move> moves) {
			anims.emplace_back(players, dims.y / 4, board_size, setup, moves);
			anims.back().setPosition(dims / 2.f);
		};
		add_anim(1, tut1_setup, tut1_moves);
		add_anim(2, tut2_setup, tut2_moves);
	}

	void mouseMove(sf::Vector2f mouse) override {
		if (exit.getBounds().contains(mouse)) {
			exit.setColor(sf::Color::Yellow);
		}
		else {
			exit.setColor(sf::Color::Red);
		}
	}

	state_transitions::StateChangeEvent onClick(sf::Vector2f mouse) override {
		if (exit.getBounds().contains(mouse)) {
			return state_transitions::ReturnToMain{};
		}
		else if (prev_tut.getGlobalBounds().contains(mouse)) {
			current = (current == 0) ? anims.size() - 1 : (current - 1);
			anims[current].reset();
		}
		else if (next_tut.getGlobalBounds().contains(mouse)) {
			current = (current + 1) % anims.size();
			anims[current].reset();
		}
		return {};
	}

	void update() override {
		anims[current].update();
	}

	void draw(sf::RenderTarget& target, sf::RenderStates states) const override {
		target.draw(exit, states);
		target.draw(anims[current], states);
		target.draw(prev_tut, states);
		target.draw(next_tut, states);
	}
};

int main()
{
	sf::ContextSettings settings;
	settings.antialiasingLevel = 2;
	settings.majorVersion = 3;
	settings.minorVersion = 2;
	settings.sRgbCapable = true;

	sf::RenderWindow window(sf::VideoMode(800, 600), "Exploding Tiles", sf::Style::Titlebar | sf::Style::Close, settings);

	sf::Shader s;
	{
		sf::MemoryInputStream stream;
		stream.open(board_shader.data(), board_shader.size());
		s.loadFromStream(stream, sf::Shader::Type::Fragment);
	}
	s.setUniform("board", sf::Shader::CurrentTexture);
	VisualBoard::shader = &s;

	sf::Shader sRGB_to_linear;
	{
		sf::MemoryInputStream stream;
		stream.open(default_color_shader.data(), default_color_shader.size());
		sRGB_to_linear.loadFromStream(stream, sf::Shader::Type::Vertex);
	}

	window.setFramerateLimit(60);

	std::unique_ptr<State> game = std::make_unique<MainMenu>(sf::Vector2f(800, 600));

	while (window.isOpen())
	{
		sf::Event event;
		while (window.pollEvent(event))
		{
			switch (event.type) {
			case sf::Event::Closed:
				window.close();
				return 0;
			case sf::Event::MouseButtonReleased:
			{
				auto event = game->onClick(sf::Vector2f{ sf::Mouse::getPosition(window) });;
				std::visit(overloaded{
						[&](state_transitions::OpenPlayerSelect) {
							game = std::make_unique<PlayerSelect>(sf::Vector2f(800,600));
						},
						[&](state_transitions::OpenTutorial) {
							game = std::make_unique<TutorialState>(sf::Vector2f(800,600));
						},
						[&](state_transitions::StartGame& g) {
							game = std::make_unique<GameState>(sf::Vector2f(400,300),250.f,g);
						},
						[&](state_transitions::ReturnToMain) {
							game = std::make_unique<MainMenu>(sf::Vector2f(800,600));
						},
						[](auto) {}
					}, event);
			}
				break;
			case sf::Event::MouseMoved:
				game->mouseMove(sf::Vector2f{ sf::Mouse::getPosition(window) });
				break;
			default:
				break;
			}
		}

		game->update();

		window.clear(sf::Color(0x1a1a64FF));

		window.draw(*game,&sRGB_to_linear);

		window.display();
	}

	return 0;
}
