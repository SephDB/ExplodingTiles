#include <string_view>
#include <iostream>
#include <optional>
#include <vector>
#include <memory>
#include <cmath>
#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>

#include "coords.hpp"
#include "board.hpp"
#include "player.hpp"
#include "shapes.hpp"

std::string_view board_shader = R"(
const float edge_thickness = 0.04;
const vec3 edge_color = vec3(1);
const vec3 highlight_color = vec3(1,0,1);

uniform int hex_size;
uniform ivec3 selected;

float min3(vec3 v) {
	return min(min(v.x,v.y),v.z);
}

float max3(vec3 v) {
	return max(max(v.x,v.y),v.z);
}

void main()
{
	vec3 min_bound = vec3(1);
	vec3 max_bound = vec3(hex_size*2+1);
	vec3 bound_edge = vec3(edge_thickness*2);

	vec3 coordinates = (gl_Color.rgb) * (hex_size+1) * 3;
	ivec3 coords = floor(coordinates);
	
	ivec3 larger_selected = selected + ivec3(1);
	
	
	if(all(greaterThan(coordinates,min_bound)) && all(lessThan(coordinates,max_bound))) {
		//Inner edge
		vec3 distance = min(fract(coordinates),ceil(coordinates)-coordinates);
		if(larger_selected == floor(coordinates)) {
			float mix = smoothstep(0,edge_thickness/2,min3(distance));
			gl_FragColor.rgb = edge_color * (1-mix) + mix*highlight_color;
			gl_FragColor.a = 1-smoothstep(edge_thickness/2,edge_thickness*1.5,min3(distance))*0.3;
		} else {
			gl_FragColor.rgb = edge_color;
			gl_FragColor.a = 1-smoothstep(edge_thickness*0.8,edge_thickness,min3(distance));
		}
	} else if(all(greaterThan(coordinates,min_bound-bound_edge)) && all(lessThan(coordinates,max_bound+bound_edge))) {
		//Outer edge
		vec3 distance = max(min_bound - coordinates, coordinates - max_bound);
		gl_FragColor.rgb = edge_color;
		gl_FragColor.a = 1 - smoothstep(edge_thickness*1.5,edge_thickness*2,max3(distance));
	} else {
		gl_FragColor = 0;
	}
}
)";

float dot(sf::Vector2f a, sf::Vector2f b) {
	return a.x * b.x + a.y * b.y;
}

template<typename Vec>
Vec lerp(Vec a, Vec b, float val) {
	return a + (b - a) * val;
}


class BoardWithPlayers {
	Board board;
	int current_player = 0;
	std::vector<std::unique_ptr<Player>> players{};

	void makeMove(TriCoord c) {
		if (!board.incTile(c, current_player)) return;
		if (!board.needsUpdate())
			nextPlayer();
	}

	void nextPlayer() {
		current_player = (current_player + 1) % players.size();
		players[current_player]->startTurn(board, current_player);
	}

public:
	BoardWithPlayers(int size) : board(size) {}

	void addPlayer(std::unique_ptr<Player> player) {
		players.push_back(std::move(player));
		if (players.size() == 1) {
			players[0]->startTurn(board, 0);
		}
	}

	const Board& getBoard() const { return board; }
	int getCurrentPlayerNum() const { return current_player; }

	Player& getCurrentPlayer() { return *players[current_player]; }
	const Player& getCurrentPlayer() const { return *players[current_player]; }

	void update() {
		if (board.needsUpdate()) {
			board.update_step();
			if (!board.needsUpdate()) nextPlayer();
		}
		else if (auto m = players[current_player]->update(); m) {
				makeMove(*m);
		}
	}

	void reset() {
		board = { board.size() };
		current_player = 0;
		players[current_player]->startTurn(board, current_player);
	}

	std::optional<int> getWinner() const {
		std::size_t total = 0;
		int player = -1;
		bool multiple_players = false;
		board.iterTiles([&](TriCoord c) {
			auto board_player = board[c].player;
			if (board_player != -1) {
				total += board[c].num;
				if (player == -1) player = board_player;
				if (player != board_player) {
					multiple_players = true;
					return false;
				}
			}
			return true;
		});
		if (multiple_players || total <= players.size()) return std::nullopt;
		return player;
	}
};

class ScoreBar : public sf::Drawable {
	std::vector<std::pair<sf::Color,float>> players;
	sf::FloatRect bar;
public:
	ScoreBar(sf::FloatRect loc) : bar(loc) {}

	void addPlayer(sf::Color c) {
		players.emplace_back(c, 0.f);
	}

	void update(const Board& b) {
		std::vector<float> player_counts(players.size());
		b.iterTiles([&](TriCoord c) {
			if (b[c].num > 0)
				player_counts[b[c].player] += b[c].num;
			return true; });
		for (size_t p = 0; p < players.size(); ++p) {
			players[p].second = lerp<float>(players[p].second, player_counts[p], 0.3f);
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
	using StartGame = std::vector<PlayerInfo>;
	struct ReturnToMain {};
	struct OpenPlayerSelect {};
	using StateChangeEvent = std::variant<std::monostate, OpenPlayerSelect, StartGame, ReturnToMain>;
}

class State : public sf::Drawable {
public:
	virtual void update() = 0;
	virtual void mouseMove(sf::Vector2f mouse) = 0;
	virtual state_transitions::StateChangeEvent onClick(sf::Vector2f mouse) = 0;
};

class GameState : public State {
	sf::Vertex outer[3];
	sf::Shader shader;

	sf::Vector2f inner[3];
	BoardWithPlayers board;

	sf::CircleShape explode;
	sf::Clock explode_timer;

	sf::VertexArray reset_arrow;

	sf::Transform show_current_player;
	sf::Transform show_winner;
	std::vector<sf::CircleShape> players;

	ScoreBar bar;
	CrossShape exit;
	
	TriCoord mouseToBoard(sf::Vector2f mouse) const {
		float length = inner[1].y - inner[0].y;
		float v1 = 1 - dot(mouse - inner[0], sf::Vector2f(0, 1)) / length;
		float v2 = 1 - dot(mouse - inner[1], inner[2] + (inner[0] - inner[2]) / 2.f - inner[1]) / (length * length);

		sf::Vector3i bary = sf::Vector3i(3.f * board.getBoard().size() * sf::Vector3f(v1, v2, 1 - v1 - v2));

		//ensure out of bounds coordinate when a coordinate < 0, converting to int != floor. Subtract one if a coordinate was below 0
		bary -= sf::Vector3i(v1 < 0, v2 < 0, v1 + v2 > 1);

		return TriCoord(bary, board.getBoard().size());
	}

	void addPlayer(int polygon_n, sf::Color color, std::unique_ptr<Player> controller) {
		board.addPlayer(std::move(controller));
		bar.addPlayer(color);
		float radius = (inner[1].y - inner[0].y) / 3;
		players.push_back(playerShape(polygon_n, color, radius / (board.getBoard().size() * 6 + 3)));
	}

public:
	GameState(sf::Vector2f center, float radius, int board_size, const state_transitions::StartGame& game_info) 
		: board(board_size), bar({ center - sf::Vector2f(radius, radius), sf::Vector2f(2 * radius, radius * 0.1f) }), exit(sf::Color::Red,40) {
		
		exit.setPosition(60, 60);
		exit.setRotation(45);

		center.y += radius * 0.2f;
		radius *= .9f;

		const float r = radius * (board_size + 1) / board_size; //Widen for outer edge
		const float sqrt3 = std::sqrt(3.f);
		outer[0] = sf::Vertex({ center.x, center.y - 2 * r }, sf::Color::Red);
		inner[0] = { center.x, center.y - 2 * radius };
		outer[1] = sf::Vertex({ center.x + r * sqrt3, center.y + r }, sf::Color::Green);
		inner[1] = { center.x + radius * sqrt3,center.y + radius };
		outer[2] = sf::Vertex({ center.x - r * sqrt3, center.y + r }, sf::Color::Blue);
		inner[2] = { center.x - radius * sqrt3,center.y + radius };

		{
			sf::MemoryInputStream stream;
			stream.open(board_shader.data(), board_shader.size());
			shader.loadFromStream(stream, sf::Shader::Type::Fragment);
		}

		shader.setUniform("hex_size", board_size);
		shader.setUniform("selected", sf::Vector3i());

		const float size = radius / (board_size * 6 + 3);

		explode = sf::CircleShape(size * 3);
		explode.setFillColor(sf::Color::Yellow);
		explode.setOutlineColor(sf::Color::Red);
		explode.setOutlineThickness(-size / 2);
		explode.setOrigin(size * 3, size * 3);

		sf::Transform rot = sf::Transform().rotate(-60);

		sf::Vector2f extra_offset{ 0, radius*1.4f };

		extra_offset = rot.transformPoint(extra_offset);

		show_current_player = sf::Transform().translate(center - extra_offset).scale(sf::Vector2f(1 / size, 1 / size) * (radius / 8));
		show_winner = sf::Transform().translate(center).scale(sf::Vector2f(1, 1) / size * radius);

		extra_offset = rot.transformPoint(extra_offset);

		reset_arrow = circArrow(center - extra_offset, sf::Color::White, 15, 24, 5);

		for (auto& [num, color, behavior] : game_info) {
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
		board.getCurrentPlayer().onInput(input_events::MouseMove{ mouseToBoard(mouse) });
	}

	state_transitions::StateChangeEvent onClick(sf::Vector2f mouse) override {
		if (reset_arrow.getBounds().contains(mouse)) {
			board.reset();
			bar.reset();
		}
		else if (exit.getBounds().contains(mouse)) {
			return state_transitions::ReturnToMain{};
		}
		else {
			board.getCurrentPlayer().onInput(input_events::MouseClick{ mouseToBoard(mouse) });
		}
		return {}; //no transitions yet
	}

	void update() override {
		if (not board.getWinner() && (not board.getBoard().needsUpdate() || explode_timer.getElapsedTime().asSeconds() > explosion_length)) {
			board.update();
			explode_timer.restart();
		}
		shader.setUniform("selected", board.getCurrentPlayer().selected().bary(board.getBoard().size()));
		bar.update(board.getBoard());
	}

	void draw(sf::RenderTarget& target, sf::RenderStates states) const override {
		target.draw(outer, 3, sf::PrimitiveType::Triangles, { states.blendMode, states.transform, states.texture, &shader });

		const float explosion_progress = explode_timer.getElapsedTime().asSeconds() / explosion_length;

		auto draw_tile = [this, explosion_progress, &target](TriCoord c, sf::RenderStates states) {
			auto s = board.getBoard()[c];
			if (s.num == 0) return;

			auto [x, y, z] = c.tri_center(board.getBoard().size());

			auto center = x * inner[0] + y * inner[1] + z * inner[2];
			
			states.transform.translate(center);
			
			const sf::CircleShape& circle = players[s.player];

			if (s.num > board.getBoard().allowedPieces(c)) {
				auto explode_state = states;
				float scale = lerp(0.3f, 1.0f, explosion_progress);
				explode_state.transform.scale(scale,scale);
				target.draw(explode, explode_state);
				for (const auto& n : c.neighbors()) {
					if (board.getBoard().inBounds(n)) {
						auto [x2, y2, z2] = n.tri_center(board.getBoard().size());
						auto move_target = x2 * inner[0] + y2 * inner[1] + z2 * inner[2] - center;
						auto move_state = states;
						move_state.transform.translate(lerp(move_target/3.f,move_target,explosion_progress));
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

		if (auto player = board.getWinner(); player) {
			target.draw(players[*player], { show_current_player });
			target.draw(players[*player], { show_winner });
		}
		else {
			board.getBoard().iterTiles([&](auto c) {draw_tile(c, states); return true; });
			target.draw(players[board.getCurrentPlayerNum()], { show_current_player });
		}

		target.draw(reset_arrow, states);
		target.draw(bar, states);
		target.draw(exit, states);
	}
};

class MainMenu : public State {
	sf::CircleShape play_button;
public:
	MainMenu(sf::Vector2f dims) : play_button(dims.x/20,3) {
		play_button.setFillColor(sf::Color::Yellow);
		play_button.setRotation(-30);
		play_button.setOrigin(sf::Vector2f(dims.x / 20, dims.x / 20));
		play_button.setPosition(dims / 2.f);
	}

	void mouseMove(sf::Vector2f mouse) override {
	}

	state_transitions::StateChangeEvent onClick(sf::Vector2f mouse) override {
		if (play_button.getGlobalBounds().contains(mouse)) {
			return state_transitions::OpenPlayerSelect{};
		}
		return {};
	}

	void update() override {

	}

	void draw(sf::RenderTarget& target, sf::RenderStates states) const override {
		target.draw(play_button);
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
	AIPlayerShape AI_shape;
	sf::CircleShape player_shape;
	ShapeSelector shape_selector;
	ColorSelector color_selector;
	state_transitions::PlayerInfo player;
	sf::RectangleShape selector;

	void refresh() {
		player_shape = playerShape(player.shape_points, player.color, outline.getSize().x / 4);
		player_shape.setPosition(outline.getSize().x / 2, player_shape.getRadius() + outline.getSize().y / 10);

		shape_selector.setPosition(0, player_shape.getPosition().y + player_shape.getRadius() + 20);
		auto shape_pos = shape_selector.getBounds();
		color_selector.setPosition(0, shape_pos.top + shape_pos.height + 20);
		
		sf::FloatRect selector_pos = (player.playerBehavior == PlayerType::Mouse) ? human_shape.getBounds() : AI_shape.getBounds();
		selector.setPosition(selector_pos.left - selector_pos.width*0.1f,selector_pos.top - selector_pos.height * 0.1f);
		selector.setSize(sf::Vector2f(selector_pos.width*1.2f, selector_pos.height*1.2f));
	}

public:
	PlayerSelector(sf::Vector2f size, state_transitions::PlayerInfo info) : outline(size), human_shape(size.x / 3), AI_shape(size.x / 3), shape_selector(size.x, info.shape_points), color_selector(size.x,info.color), player(info) {
		outline.setOutlineColor(sf::Color::Cyan);
		outline.setFillColor(sf::Color::Transparent);
		outline.setOutlineThickness(2.f);

		human_shape.setPosition(0, size.y - human_shape.getBounds().height - 20);
		AI_shape.setPosition(human_shape.getPosition() + sf::Vector2f(size.x / 2, 0));
		selector.setFillColor(sf::Color::Magenta);
		refresh();
	}

	void onMouseClick(sf::Vector2f mouse) {
		mouse = getInverseTransform().transformPoint(mouse);
		if (human_shape.getBounds().contains(mouse)) {
			player.playerBehavior = PlayerType::Mouse;
			refresh();
		}
		else if (AI_shape.getBounds().contains(mouse)) {
			player.playerBehavior = PlayerType::AIRando;
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
	}

	operator state_transitions::PlayerInfo() const {
		return player;
	}

	sf::FloatRect getBounds() const {
		return getTransform().transformRect(outline.getGlobalBounds());
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
	}
};

class PlayerSelect : public State {
	CrossShape add_player;
	sf::CircleShape play_button;
	std::vector<PlayerSelector> players;
	sf::Vector2f dims;
	
	sf::Vector2f player_select_size() const {
		return { dims.x / 6,dims.y / 2 };
	}

	void updateLayout() {
		const auto size = player_select_size();
		const float padding = 20;

		float left = dims.x / 2 - (size.x+padding) * players.size() / 2;
		for (auto& p : players) {
			p.setPosition({ left,dims.y / 2 - size.y / 2 });
			left += padding + size.x;
		}
		add_player.setPosition({ left + add_player.getBounds().width/2, dims.y / 2 });
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
	PlayerSelect(sf::Vector2f dims) : add_player(sf::Color::Green,dims.x/20), play_button(dims.x / 20, 3), dims(dims) {
		play_button.setFillColor(sf::Color::Yellow);
		play_button.setRotation(-30);
		play_button.setOrigin(sf::Vector2f(play_button.getRadius(),play_button.getRadius()));
		play_button.setPosition(dims / 2.f + sf::Vector2f(0,play_button.getRadius() + player_select_size().y/2+20));

		nextPlayer();
	}

	void mouseMove(sf::Vector2f mouse) override {
	}

	state_transitions::StateChangeEvent onClick(sf::Vector2f mouse) override {
		if (play_button.getGlobalBounds().contains(mouse)) {
			state_transitions::StartGame ret;
			std::copy(players.begin(), players.end(), std::back_inserter(ret));
			return ret;
		} else if (add_player.getBounds().contains(mouse)) {
			nextPlayer();
		} else {
			for (auto& p : players) {
				if (p.getBounds().contains(mouse)) {
					p.onMouseClick(mouse);
					break;
				}
			}
		}
		return {};
	}

	void update() override {

	}

	void draw(sf::RenderTarget& target, sf::RenderStates states) const override {
		target.draw(add_player,states);
		target.draw(play_button,states);
		for (auto& p : players) {
			target.draw(p,states);
		}
	}

};

int main()
{
	sf::RenderWindow window(sf::VideoMode(800, 600), "Exploding Tiles", sf::Style::Default, sf::ContextSettings(0,0,2));

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
						[&](state_transitions::StartGame& g) {
							game = std::make_unique<GameState>(sf::Vector2f(400,300),250.f,3,g);
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

		window.clear(sf::Color(0x4A4AB5FF));

		window.draw(*game);

		window.display();
	}

	return 0;
}
