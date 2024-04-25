#pragma once
#include <string> 
#include <vector>
#include "config.hpp"

#include "leif.h"

struct InfoCard {
  std::string title;

  bool operator==(const InfoCard& other) const {
    return title == other.title;
  }

  float width = 0.0f;

  float showTime = 3.0f, showTimer = 0.0f;

  LfColor bgColor, textColor;
};

class InfoCardHandler {
  public:
    InfoCardHandler() = default;
    InfoCardHandler(float cardPadding, float cardH);

    void addCard(const std::string& title, LfColor bgColor = lf_color_brightness(GRAY, 0.6f), LfColor textColor = LF_WHITE);

    void render();
    void update();
  private:
    float _cardPadding = 0, _cardH;
    std::vector<InfoCard> _infoCards{};
};
