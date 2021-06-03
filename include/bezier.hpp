#pragma once

#include <vector>
#include <SFML/System/Vector2.hpp>
#include "vectorops.hpp"

template<std::size_t Degree>
struct Bezier {
	//first control point fixed to origin
	std::array<sf::Vector2f, Degree> points;

	sf::Vector2f value(float t) const {
		Bezier<Degree - 1> lowered;
		sf::Vector2f offset = points[0] * t;
		for (std::size_t i = 0; i < Degree - 1; ++i) {
			lowered.points[i] = lerp(points[i], points[i + 1], t) - offset;
		}
		return lowered.value(t) + offset;
	}

	sf::Vector2f tangent(float t) const {
		Bezier<Degree - 1> lowered;
		const float order_f = static_cast<float>(Degree);
		sf::Vector2f offset = points[0] * order_f;
		for (std::size_t i = 0; i < Degree - 1; ++i) {
			lowered.points[i] = (points[i + 1] - points[i]) * order_f - offset;
		}
		return normalized(lowered.value(t) + offset);
	}
};

template<>
struct Bezier<1> {
	std::array<sf::Vector2f, 1> points;

	sf::Vector2f value(float t) const {
		return points[0] * t;
	}
};

using CubeSpline = Bezier<3>;


/**
* Stitches Beziers together by their endpoints, properly aligning the tangent values for smooth interpolation
*/
class PolyBezier {
	//Helper classes
	struct GenericSpline {
		GenericSpline(std::size_t s) : start(s) {}
		std::size_t start;
		//Construct the spline starting at points[start], get value/tangent at t
		virtual sf::Vector2f value(float t, std::span<const sf::Vector2f> points) const = 0;
		virtual sf::Vector2f tangent(float t, std::span<const sf::Vector2f> points) const = 0;
	};

	template<std::size_t Degree>
	struct ConcreteSpline final : GenericSpline {
		ConcreteSpline(std::size_t start) : GenericSpline(start) {}

		Bezier<Degree> get(std::span<const sf::Vector2f> points) const {
			sf::Vector2f offset = points[start];
			Bezier<Degree> b;
			for (std::size_t i = 0; i < Degree; ++i) {
				b.points[i] = points[start + i + 1] - offset;
			}
			return b;
		}

		sf::Vector2f value(float t, std::span<const sf::Vector2f> points) const override {
			return get(points).value(t) + points[start];
		}

		sf::Vector2f tangent(float t, std::span<const sf::Vector2f> points) const override {
			return get(points).tangent(t);
		}
	};

	struct ErasedSpline {
		//Using GenericSpline as size and alignment since ConcreteSpline has no additional data
		std::aligned_storage_t<sizeof(GenericSpline), alignof(GenericSpline)> storage;

		template<std::size_t Degree>
		ErasedSpline(ConcreteSpline<Degree> s) {
			new(&storage) ConcreteSpline<Degree>(s);
		}

		//default copy and move ops do the right thing, copying the vtable pointer and size member

		const GenericSpline& get() const {
			return *reinterpret_cast<const GenericSpline*>(&storage);
		}

		~ErasedSpline() {
			get().~GenericSpline();
		}
	};

	std::vector<sf::Vector2f> control_points;
	std::vector<ErasedSpline> splines;

	std::size_t num_curves() const {
		return splines.size();
	}

	std::pair<std::size_t, float> get_loc(float t) const {
		std::size_t n = static_cast<std::size_t>(t * num_curves());
		if (n == num_curves()) n--;

		t = inverseLerp<float>(n, n + 1, t * num_curves());
		return { n,t };
	}

public:
	template<std::size_t Degree>
	PolyBezier(Bezier<Degree> s) : control_points(s.points.begin(), s.points.end()) {
		control_points.insert(control_points.begin(), sf::Vector2f()); //insert 0,0
		splines.emplace_back(ConcreteSpline<Degree>(0));
	}

	template<std::size_t Degree>
	PolyBezier& addSpline(Bezier<Degree> s) {
		auto end_tangent = tangent(1);
		auto start_tangent = s.tangent(0);
		auto cos = dot(end_tangent, start_tangent);
		auto sin = (end_tangent.x * start_tangent.y - end_tangent.y * start_tangent.x);
		//rotates the start tangent of the new spline onto the end tangent of the current end
		sf::Transform rot = sf::Transform(cos,-sin,0,
		                                  sin,cos,0,
		                                  0,0,1);

		sf::Vector2f offset = control_points.back();

		splines.emplace_back(ConcreteSpline<Degree>{control_points.size() - 1});

		for (const auto& p : s.points) {
			control_points.push_back(rot.transformPoint(p) + offset);
		}

		return *this;
	}

	sf::Vector2f value(float t) const {
		auto [n, t2] = get_loc(t);

		return splines[n].get().value(t2,control_points);
	}

	sf::Vector2f tangent(float t) const {
		auto [n, t2] = get_loc(t);

		return splines[n].get().tangent(t2,control_points);
	}
};


sf::VertexArray curve_to_strip(const auto& curve, float thickness, std::size_t num_points = 100) {
	sf::VertexArray ret(sf::PrimitiveType::TrianglesStrip, num_points*2);
	for (std::size_t i = 0; i < num_points; ++i) {
		const float t = static_cast<float>(i) / (num_points - 1);
		const auto midpoint = curve.value(t);
		const auto tangent = curve.tangent(t);
		const auto normal = sf::Vector2f{ -tangent.y,tangent.x };
		ret[i * 2] = midpoint + normal * thickness;
		ret[i * 2 + 1] = midpoint - normal * thickness;
	}
	return ret;
}
