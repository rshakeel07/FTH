#pragma once

bool startWebServer();
void serviceWebServer();
void publishTelemetry(
	unsigned long uptimeMs,
	int bpm,
	bool imuJerking,
	bool leftBuzzerOn,
	bool rightBuzzerOn
);