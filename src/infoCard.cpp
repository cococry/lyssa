#include "infoCard.hpp"
#include "global.hpp"

#include "leif.h" 

InfoCardHandler::InfoCardHandler(float cardPadding, float cardH) 
  : _cardPadding(cardPadding), _cardH(cardH) {
}

void InfoCardHandler::addCard(const std::string& title, const std::string& desc, LfColor bgColor, LfColor textColor) {
  _infoCards.push_back((InfoCard){
      .title = title, 
      .desc = desc, 
      .width = (MAX(lf_text_dimension(title.c_str()).x, lf_text_dimension(desc.c_str()).x)) + _cardPadding,
      .bgColor = bgColor,
      .textColor = textColor
      });
}
void InfoCardHandler::render() {
  float yOffset = 0.0f;
  for(auto& infoCard : _infoCards) {
    // Div
    {
      LfUIElementProps props = lf_get_theme().div_props;
      props.color = infoCard.bgColor;
      props.corner_radius = 4.0f;
      lf_push_style_props(props);
      lf_div_begin(((vec2s){
            state.win->getWidth() - infoCard.width - _cardPadding,
            state.win->getHeight() - _cardH - _cardPadding - yOffset}), 
          ((vec2s){infoCard.width, _cardH}), false);
    }
    // Content
    {
      LfUIElementProps props = lf_get_theme().text_props;
      props.text_color = infoCard.textColor;
      lf_push_style_props(props);
      lf_text(infoCard.title.c_str());
      lf_pop_style_props();

      lf_next_line();

      props.text_color = lf_color_brightness(infoCard.textColor, 0.6f);
      lf_push_style_props(props);
      lf_text(infoCard.desc.c_str());
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
    printf("%f\n", infoCard.showTimer);
    if(infoCard.showTimer >= infoCard.showTime) {
      _infoCards.erase(std::find(_infoCards.begin(), _infoCards.end(), infoCard));
    }
  }
}
