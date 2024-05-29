#include "infoCard.hpp"
#include "config.hpp"
#include "global.hpp"
#include <algorithm>
extern "C" {
#include <leif/leif.h>
}

InfoCardHandler::InfoCardHandler(float cardPadding, float cardH) 
  : _cardPadding(cardPadding), _cardH(cardH) {
  }

void InfoCardHandler::addCard(const std::string& title, LfColor bgColor, LfColor textColor) {
  if(_infoCards.size() + 1 <= MAX_INFO_CARDS) {
    _infoCards.push_back((InfoCard){
        .title = title, 
        .width = lf_text_dimension(title.c_str()).x + _cardPadding + 50.0f,
        .bgColor = bgColor,
        .textColor = textColor
        });
  }
}
void InfoCardHandler::render() {
  float yOffset = 0.0f;
  for(auto& infoCard : _infoCards) {
    // Div
    {
      LfUIElementProps props = lf_get_theme().div_props;
      props.color = infoCard.bgColor;
      props.corner_radius = 6.0f;
      lf_push_style_props(props);
      lf_div_begin(((vec2s){
            state.win->getWidth() - infoCard.width - _cardPadding,
            state.win->getHeight() - _cardH - _cardPadding - yOffset}), 
          ((vec2s){infoCard.width, _cardH}), false);
    }
    // Info icon
    {
      LfUIElementProps props = lf_get_theme().image_props;
      lf_push_style_props(props);
      props.margin_left = 15;
      props.margin_top = 10;
      lf_set_image_color(infoCard.textColor);
      lf_image((LfTexture){.id = state.icons["info"].id, .width = 20, .height = 20});
      lf_unset_image_color();
      lf_pop_style_props();
    }
    // Content
    {
      LfUIElementProps props = lf_get_theme().text_props;
      props.text_color = infoCard.textColor;
      lf_push_style_props(props);
      lf_text(infoCard.title.c_str());
      lf_pop_style_props();
    }

    lf_div_end();
    lf_pop_style_props();
    yOffset += _cardH + _cardPadding;
  }
}

void InfoCardHandler::update() {
  for(auto& infoCard : _infoCards) {
    infoCard.showTimer += state.deltaTime;
  }
  for(auto& infoCard : _infoCards) {
    if(infoCard.showTimer >= infoCard.showTime) {
      _infoCards.erase(std::find(_infoCards.begin(), _infoCards.end(), infoCard));
    }
  }
}
