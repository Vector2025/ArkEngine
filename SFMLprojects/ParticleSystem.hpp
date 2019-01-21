#pragma once

#include <SFML/System/Time.hpp>
#include <SFML/Graphics.hpp>
#include <vector>
#include <functional>
#include "RandomNumbers.hpp"
#include "gsl.hpp"
#include "VectorEngine.hpp"

static inline constexpr auto PI = 3.14159f;

struct Particles final : public Data<Particles> {

	COPYABLE(Particles)

	Particles(int count, sf::Time lifeTime,
	          Distribution<float> speedArgs = { 0, 0 },
	          Distribution<float> angleArgs = { 0.f, 2 * 3.14159f },
	          DistributionType lifeTimeDistType = DistributionType::uniform,
	          std::function<sf::Color()> getColor = []() { return sf::Color::White; })
	    : count(count),
	    lifeTime(lifeTime),
	    speedDistribution(speedArgs),
	    angleDistribution(angleArgs),
	    getColor(getColor)
	{ 
		lifeTimeDistribution.type = lifeTimeDistType;
		this->makeLifeTimeDist();
	}

	int count;
	sf::Time lifeTime;

	Distribution<float> speedDistribution;
	Distribution<float> angleDistribution;
	Distribution<float> lifeTimeDistribution;

	bool spawn = false;
	bool fireworks = false;
	sf::Vector2f emitter{ 0.f, 0.f };
	std::function<sf::Color()> getColor;

	// optional
	// std::unique_ptr<Transform> transform = nullptr;

	// call if lifeTime is modified
	void makeLifeTimeDist() noexcept {
		if (lifeTimeDistribution.type == DistributionType::uniform)
			this->makeLifeTimeDistUniform();
		else
			this->makeLifeTimeDistNormal();
	}
	void makeLifeTimeDistUniform(float divLowerBound = 4) noexcept { 
		lifeTimeDistribution = {
			lifeTime.asMilliseconds() / divLowerBound,
			(float)lifeTime.asMilliseconds(),
			DistributionType::uniform };
	}
	void makeLifeTimeDistNormal() noexcept {
		lifeTimeDistribution = {
			lifeTime.asMilliseconds() / 2.f,
			lifeTime.asMilliseconds() / 2.f,
			DistributionType::normal };
	}
};

// templates

static auto makeRed = []() {
	auto green = RandomNumber<uint32_t>(0, 150);
	return sf::Color(255, green, 0);
};

static auto makeGreen = []() {
	auto red = RandomNumber<uint32_t>(0, 150);
	return sf::Color(red, 255, 0);
};

static auto makeBrightBlue = []() {
	auto green = RandomNumber<uint32_t>(100, 255);
	return sf::Color(0, green, 255);
};

static auto makeDarkBlue = []() {
	auto green = RandomNumber<uint32_t>(20, 150);
	return sf::Color(10, green, 240);
};

static auto makeColor = []() {
	return sf::Color(RandomNumber<uint32_t>(0x000000ff, 0xffffffff));
};

inline Particles getFireParticles() 
{
	Particles fireParticles = { 1000, sf::seconds(2), { 1, 100 } };
	fireParticles.getColor = makeRed;
	return fireParticles;
}

inline Particles getRainbowParticles()
{
	Particles rainbowParticles(2000, sf::seconds(3), { 1, 100 , DistributionType::normal});
	rainbowParticles.getColor = makeColor;
	return rainbowParticles;
}

inline Particles getGreenParticles()
{
	auto greenParticles = getFireParticles();
	greenParticles.getColor = makeGreen;
	return greenParticles;
}

class ParticleSystem final : public System {

public:
	static inline sf::Vector2f gravityVector{ 0.f, 0.f };
	static inline sf::Vector2f gravityPoint{ 0.f, 0.f };
	static inline float gravityMagnitude = 20;
	static inline bool hasUniversalGravity = true;

private:
	void init() override {
		this->initFrom<Particles>();
	}

	void update() override;
	void add(Component*) override;
	void remove(Component*) override;
	void render(sf::RenderTarget& target) override;

	void updateBatch(const Particles&, gsl::span<sf::Vertex>, gsl::span<sf::Vector2f>, gsl::span<sf::Time>);
	void respawnParticle(const Particles&, sf::Vertex&, sf::Vector2f&, sf::Time&);

private:
	std::vector<sf::Vertex>	vertices;
	std::vector<sf::Vector2f> velocities;
	std::vector<sf::Time> lifeTimes;
};
