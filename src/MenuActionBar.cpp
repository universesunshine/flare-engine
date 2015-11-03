/*
Copyright © 2011-2012 Clint Bellanger
Copyright © 2012 Igor Paliychuk
Copyright © 2013 Kurt Rinnert
Copyright © 2014 Henrik Andersson

This file is part of FLARE.

FLARE is free software: you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.

FLARE is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
FLARE.  If not, see http://www.gnu.org/licenses/
*/

/**
 * class MenuActionBar
 *
 * Handles the config, display, and usage of the 0-9 hotkeys, mouse buttons, and menu calls
 */

#include "CommonIncludes.h"
#include "FileParser.h"
#include "Menu.h"
#include "MenuActionBar.h"
#include "SharedGameResources.h"
#include "SharedResources.h"
#include "Settings.h"
#include "StatBlock.h"
#include "TooltipData.h"
#include "UtilsParsing.h"
#include "WidgetSlot.h"
#include "WidgetLabel.h"
#include "SharedGameResources.h"

#include <climits>

MenuActionBar::MenuActionBar(Avatar *_hero)
	: emptyslot(NULL)
	, disabled(NULL)
	, attention(NULL)
	, hero(_hero)
	, slots_count(0)
	, drag_prev_slot(-1)
	, updated(false)
	, twostep_slot(-1) {

	src.w = ICON_SIZE;
	src.h = ICON_SIZE;

	menu_labels.resize(4);

	tablist = TabList(HORIZONTAL, ACTIONBAR_BACK, ACTIONBAR_FORWARD, ACTIONBAR);

	for (unsigned int i=0; i<4; i++) {
		menus[i] = new WidgetSlot(-1, ACTIONBAR);
	}

	// Read data from config file
	FileParser infile;

	// @CLASS MenuActionBar|Description of menus/actionbar.txt
	if (infile.open("menus/actionbar.txt")) {
		while (infile.next()) {
			if (parseMenuKey(infile.key, infile.val))
				continue;

			// @ATTR slot|index(integer), x (integer), y (integer)|Index (max 10) and position for power slot.
			if (infile.key == "slot") {
				unsigned index = popFirstInt(infile.val, ',');
				if (index == 0 || index > 10) {
					infile.error("MenuActionBar: Slot index must be in range 1-10.");
				}
				else {
					int x = popFirstInt(infile.val, ',');
					int y = popFirstInt(infile.val, ',');
					addSlot(index-1, x, y);
				}
			}
			// @ATTR slot_M1|x (integer), y (integer)|Position for the primary action slot.
			else if (infile.key == "slot_M1") {
				int x = popFirstInt(infile.val, ',');
				int y = popFirstInt(infile.val, ',');
				addSlot(10, x, y);
			}
			// @ATTR slot_M2|x (integer), y (integer)|Position for the secondary action slot.
			else if (infile.key == "slot_M2") {
				int x = popFirstInt(infile.val, ',');
				int y = popFirstInt(infile.val, ',');
				addSlot(11, x, y);
			}

			// @ATTR char_menu|x (integer), y (integer)|Position for the Character menu button.
			else if (infile.key == "char_menu") {
				int x = popFirstInt(infile.val, ',');
				int y = popFirstInt(infile.val, ',');
				menus[MENU_CHARACTER]->setBasePos(x, y);
				menus[MENU_CHARACTER]->pos.w = menus[MENU_CHARACTER]->pos.h = ICON_SIZE;
			}
			// @ATTR inv_menu|x (integer), y (integer)|Position for the Inventory menu button.
			else if (infile.key == "inv_menu") {
				int x = popFirstInt(infile.val, ',');
				int y = popFirstInt(infile.val, ',');
				menus[MENU_INVENTORY]->setBasePos(x, y);
				menus[MENU_INVENTORY]->pos.w = menus[MENU_INVENTORY]->pos.h = ICON_SIZE;
			}
			// @ATTR powers_menu|x (integer), y (integer)|Position for the Powers menu button.
			else if (infile.key == "powers_menu") {
				int x = popFirstInt(infile.val, ',');
				int y = popFirstInt(infile.val, ',');
				menus[MENU_POWERS]->setBasePos(x, y);
				menus[MENU_POWERS]->pos.w = menus[MENU_POWERS]->pos.h = ICON_SIZE;
			}
			// @ATTR log_menu|x (integer), y (integer)|Position for the Log menu button.
			else if (infile.key == "log_menu") {
				int x = popFirstInt(infile.val, ',');
				int y = popFirstInt(infile.val, ',');
				menus[MENU_LOG]->setBasePos(x, y);
				menus[MENU_LOG]->pos.w = menus[MENU_LOG]->pos.h = ICON_SIZE;
			}

			else infile.error("MenuActionBar: '%s' is not a valid key.", infile.key.c_str());
		}
		infile.close();
	}

	for (unsigned int i=0; i<4; i++) {
		tablist.add(menus[i]);
	}

	slots_count = static_cast<unsigned>(slots.size());

	hotkeys.resize(slots_count);
	hotkeys_temp.resize(slots_count);
	hotkeys_mod.resize(slots_count);
	locked.resize(slots_count);
	slot_item_count.resize(slots_count);
	slot_enabled.resize(slots_count);
	slot_activated.resize(slots_count);

	clear();

	loadGraphics();

	align();

	menu_act = this;
}

void MenuActionBar::addSlot(unsigned index, int x, int y) {
	if (index >= slots.size()) {
		labels.resize(index+1);
		slots.resize(index+1, NULL);
	}

	slots[index] = new WidgetSlot(-1, ACTIONBAR);
	slots[index]->setBasePos(x, y);
	slots[index]->pos.w = slots[index]->pos.h = ICON_SIZE;
	slots[index]->continuous = true;

	tablist.add(slots[index]);
}

void MenuActionBar::align() {
	Menu::align();

	for (unsigned int i = 0; i < slots_count; i++) {
		if (slots[i]) {
			slots[i]->setPos(window_area.x, window_area.y);
		}
	}
	for (unsigned int i=0; i<4; i++) {
		menus[i]->setPos(window_area.x, window_area.y);
	}

	// set keybinding labels
	for (unsigned int i = 0; i < static_cast<unsigned int>(ACTIONBAR_MAIN); i++) {
		if (i < slots.size() && slots[i]) {
			labels[i] = msg->get("Hotkey: %s", inpt->getBindingString(i+BAR_1));
		}
	}

	for (unsigned int i=ACTIONBAR_MAIN; i < static_cast<unsigned int>(ACTIONBAR_MAX); i++) {
		if (i < slots.size() && slots[i]) {
			labels[i] = msg->get("Hotkey: %s", inpt->getBindingString(i - ACTIONBAR_MAIN + MAIN1));
		}
	}
	for (unsigned int i=0; i<menu_labels.size(); i++) {
		menus[i]->setPos(window_area.x, window_area.y);
		menu_labels[i] = msg->get("Hotkey: %s", inpt->getBindingString(i + CHARACTER));
	}
}

void MenuActionBar::clear() {
	// clear action bar
	for (unsigned int i = 0; i < slots_count; i++) {
		hotkeys[i] = 0;
		hotkeys_temp[i] = 0;
		hotkeys_mod[i] = 0;
		slot_item_count[i] = -1;
		slot_enabled[i] = true;
		locked[i] = false;
		slot_activated[i] = false;
	}

	// clear menu notifications
	for (int i=0; i<4; i++)
		requires_attention[i] = false;

	twostep_slot = -1;
}

void MenuActionBar::loadGraphics() {
	Image *graphics;

	setBackground("images/menus/actionbar_trim.png");

	graphics = render_device->loadImage("images/menus/slot_empty.png");
	if (graphics) {
		emptyslot = graphics->createSprite();
		emptyslot->setClip(0,0,ICON_SIZE,ICON_SIZE);
		graphics->unref();
	}

	graphics = render_device->loadImage("images/menus/disabled.png");
	if (graphics) {
		disabled = graphics->createSprite();
		disabled->setClip(0,0,ICON_SIZE,ICON_SIZE);
		graphics->unref();
	}

	graphics = render_device->loadImage("images/menus/attention_glow.png");
	if (graphics) {
		attention = graphics->createSprite();
		graphics->unref();
	}
}

// Renders the "needs attention" icon over the appropriate log menu
void MenuActionBar::renderAttention(int menu_id) {
	Rect dest;

	// x-value is 12 hotkeys and 4 empty slots over
	dest.x = window_area.x + (menu_id * ICON_SIZE) + ICON_SIZE*15;
	dest.y = window_area.y+3;
	dest.w = dest.h = ICON_SIZE;
	if (attention) {
		attention->setDest(dest);
		render_device->render(attention);
	}

	// put an asterisk on this icon if in colorblind mode
	if (COLORBLIND) {
		WidgetLabel label;
		label.set(dest.x + 2, dest.y + 2, JUSTIFY_LEFT, VALIGN_TOP, "*", font->getColor("menu_normal"));
		label.render();
	}
}

void MenuActionBar::logic() {
	tablist.logic();
}

void MenuActionBar::render() {

	Menu::render();

	// draw hotkeyed icons
	for (unsigned i = 0; i < slots_count; i++) {
		if (!slots[i]) continue;

		if (hotkeys[i] != 0 && static_cast<unsigned>(hotkeys_mod[i]) < powers->powers.size()) {
			const Power &power = powers->getPower(hotkeys_mod[i]);

			//see if the slot should be greyed out
			slot_enabled[i] = (hero->hero_cooldown[hotkeys_mod[i]] == 0)
							  && (slot_item_count[i] == -1 || (slot_item_count[i] > 0 && power.requires_item_quantity <= slot_item_count[i]))
							  && !hero->stats.effects.stun
							  && hero->stats.alive
							  && hero->stats.canUsePower(power, hotkeys_mod[i])
							  && (twostep_slot == -1 || static_cast<unsigned>(twostep_slot) == i);

			slots[i]->setIcon(power.icon);
			slots[i]->render();
		}
		else {
			Rect dest;
			dest.x = slots[i]->pos.x;
			dest.y = slots[i]->pos.y;
			dest.h = dest.w = ICON_SIZE;
			if (emptyslot) {
				emptyslot->setDest(dest);
				render_device->render(emptyslot);
			}
			slots[i]->renderSelection();
		}
	}

	for (int i=0; i<4; i++)
		menus[i]->render();

	renderCooldowns();

	// render log attention notifications
	for (int i=0; i<4; i++)
		if (requires_attention[i])
			renderAttention(i);
}

/**
 * Display a notification for any power on cooldown
 * Also displays disabled powers
 */
void MenuActionBar::renderCooldowns() {
	Rect item_src;

	for (unsigned i = 0; i < slots_count; i++) {
		if (slots[i] && !slot_enabled[i]) {
			item_src.x = item_src.y = 0;
			item_src.w = item_src.h = ICON_SIZE;

			// Wipe from bottom to top
			if (hero->hero_cooldown[hotkeys_mod[i]] && powers->powers[hotkeys_mod[i]].cooldown && (twostep_slot == -1 || static_cast<unsigned>(twostep_slot) == i)) {
				item_src.h = (ICON_SIZE * hero->hero_cooldown[hotkeys_mod[i]]) / powers->powers[hotkeys_mod[i]].cooldown;
			}

			if (disabled) {
				disabled->setClip(item_src);
				disabled->setDest(slots[i]->pos);
				render_device->render(disabled);
			}

			slots[i]->renderSelection();
		}
	}
}

/**
 * On mouseover, show tooltip for buttons
 */
TooltipData MenuActionBar::checkTooltip(const Point& mouse) {
	TooltipData tip;

	if (isWithin(menus[MENU_CHARACTER]->pos, mouse)) {
		if (COLORBLIND && requires_attention[MENU_CHARACTER])
			tip.addText(msg->get("Character") + " (*)");
		else
			tip.addText(msg->get("Character"));

		tip.addText(menu_labels[MENU_CHARACTER]);
		return tip;
	}
	if (isWithin(menus[MENU_INVENTORY]->pos, mouse)) {
		if (COLORBLIND && requires_attention[MENU_INVENTORY])
			tip.addText(msg->get("Inventory") + " (*)");
		else
			tip.addText(msg->get("Inventory"));

		tip.addText(menu_labels[MENU_INVENTORY]);
		return tip;
	}
	if (isWithin(menus[MENU_POWERS]->pos, mouse)) {
		if (COLORBLIND && requires_attention[MENU_POWERS])
			tip.addText(msg->get("Powers") + " (*)");
		else
			tip.addText(msg->get("Powers"));

		tip.addText(menu_labels[MENU_POWERS]);
		return tip;
	}
	if (isWithin(menus[MENU_LOG]->pos, mouse)) {
		if (COLORBLIND && requires_attention[MENU_LOG])
			tip.addText(msg->get("Log") + " (*)");
		else
			tip.addText(msg->get("Log"));

		tip.addText(menu_labels[MENU_LOG]);
		return tip;
	}
	for (unsigned i = 0; i < slots_count; i++) {
		if (slots[i] && isWithin(slots[i]->pos, mouse)) {
			if (hotkeys_mod[i] != 0) {
				tip.addText(powers->powers[hotkeys_mod[i]].name);
			}
			tip.addText(labels[i]);
		}
	}

	return tip;
}

/**
 * After dragging a power or item onto the action bar, set as new hotkey
 */
void MenuActionBar::drop(const Point& mouse, int power_index, bool rearranging) {
	for (unsigned i = 0; i < slots_count; i++) {
		if (slots[i] && isWithin(slots[i]->pos, mouse)) {
			if (rearranging) {
				if ((locked[i] && !locked[drag_prev_slot]) || (!locked[i] && locked[drag_prev_slot])) {
					locked[i] = !locked[i];
					locked[drag_prev_slot] = !locked[drag_prev_slot];
				}
				hotkeys[drag_prev_slot] = hotkeys[i];
			}
			else if (locked[i]) return;
			hotkeys[i] = power_index;
			updated = true;
			return;
		}
	}
}

/**
 * Return the power to the last clicked on slot
 */
void MenuActionBar::actionReturn(int power_index) {
	drop(last_mouse, power_index, 0);
}

/**
 * CTRL-click a hotkey to clear it
 */
void MenuActionBar::remove(const Point& mouse) {
	for (unsigned i=0; i<slots_count; i++) {
		if (slots[i] && isWithin(slots[i]->pos, mouse)) {
			if (locked[i]) return;
			hotkeys[i] = 0;
			updated = true;
			return;
		}
	}
}

/**
 * If pressing an action key (keyboard or mouseclick) and the power can be used,
 * add that power to the action queue
 */
void MenuActionBar::checkAction(std::vector<ActionData> &action_queue) {
	if (inpt->pressing[ACTIONBAR_USE] && tablist.getCurrent() == -1 && slots_count > 10) {
		tablist.setCurrent(slots[10]);
		slots[10]->in_focus = true;
	}

	// check click and hotkey actions
	for (unsigned i = 0; i < slots_count; i++) {
		ActionData action;
		action.hotkey = i;
		bool have_aim = false;
		slot_activated[i] = false;

		if (!slots[i]) continue;

		if (slot_enabled[i]) {
			// part two of two step activation
			if (static_cast<unsigned>(twostep_slot) == i && inpt->pressing[MAIN1] && !inpt->lock[MAIN1]) {
				have_aim = true;
				action.power = hotkeys_mod[i];
				twostep_slot = -1;
				inpt->lock[MAIN1] = true;
			}

			// mouse/touch click
			else if (!NO_MOUSE && slots[i]->checkClick() == ACTIVATED) {
				have_aim = false;
				slot_activated[i] = true;
				action.power = hotkeys_mod[i];

				// if a power requires a fixed target (like teleportation), break up activation into two parts
				// the first step is to mark the slot that was clicked on
				if (action.power > 0) {
					const Power &power = powers->getPower(action.power);
					if (power.starting_pos == STARTING_POS_TARGET || power.buff_teleport) {
						twostep_slot = i;
						action.power = 0;
					}
					else {
						twostep_slot = -1;
					}
				}
			}

			// joystick/keyboard action button
			else if (inpt->pressing[ACTIONBAR_USE] && static_cast<unsigned>(tablist.getCurrent()) == i) {
				have_aim = false;
				slot_activated[i] = true;
				action.power = hotkeys_mod[i];
				twostep_slot = -1;
			}

			// pressing hotkey
			else if (i<10 && inpt->pressing[i+BAR_1]) {
				have_aim = true;
				action.power = hotkeys_mod[i];
				twostep_slot = -1;
			}
			else if (i==10 && inpt->pressing[MAIN1] && !inpt->lock[MAIN1] && !isWithin(window_area, inpt->mouse)) {
				have_aim = true;
				action.power = hotkeys_mod[10];
				twostep_slot = -1;
			}
			else if (i==11 && inpt->pressing[MAIN2] && !inpt->lock[MAIN2] && !isWithin(window_area, inpt->mouse)) {
				have_aim = true;
				action.power = hotkeys_mod[11];
				twostep_slot = -1;
			}
		}

		// a power slot was activated
		if (action.power > 0 && static_cast<unsigned>(action.power) < powers->powers.size()) {
			const Power &power = powers->getPower(action.power);
			bool can_use_power = true;
			action.instant_item = (power.new_state == POWSTATE_INSTANT && power.requires_item > 0);

			// check if we can add this power to the action queue
			for (unsigned j=0; j<action_queue.size(); j++) {
				if (action_queue[j].hotkey == i) {
					// this power is already in the action queue, update its target
					action_queue[j].target = setTarget(have_aim, power.aim_assist);
					can_use_power = false;
					break;
				}
				else if (!action.instant_item && !action_queue[j].instant_item) {
					can_use_power = false;
					break;
				}
			}
			if (!can_use_power)
				continue;

			// set the target depending on how the power was triggered
			action.target = setTarget(have_aim, power.aim_assist);

			// add it to the queue
			action_queue.push_back(action);
		}
		else {
			// if we're not triggering an action that is currently in the queue,
			// remove it from the queue
			for (unsigned j=0; j<action_queue.size(); j++) {
				if (action_queue[j].hotkey == i)
					action_queue.erase(action_queue.begin()+j);
			}
		}
	}
}

/**
 * If clicking while a menu is open, assume the player wants to rearrange the action bar
 */
int MenuActionBar::checkDrag(const Point& mouse) {
	int power_index;

	for (unsigned i=0; i<slots_count; i++) {
		if (slots[i] && isWithin(slots[i]->pos, mouse)) {
			drag_prev_slot = i;
			power_index = hotkeys[i];
			hotkeys[i] = 0;
			last_mouse = mouse;
			updated = true;
			return power_index;
		}
	}

	return 0;
}

/**
 * if clicking a menu, act as if the player pressed that menu's hotkey
 */
void MenuActionBar::checkMenu(bool &menu_c, bool &menu_i, bool &menu_p, bool &menu_l) {
	if (menus[MENU_CHARACTER]->checkClick()) {
		menu_c = true;
	}
	else if (menus[MENU_INVENTORY]->checkClick()) {
		menu_i = true;
	}
	else if (menus[MENU_POWERS]->checkClick()) {
		menu_p = true;
	}
	else if (menus[MENU_LOG]->checkClick()) {
		menu_l = true;
	}

	// also allow ACTIONBAR_USE to open menus
	if (inpt->pressing[ACTIONBAR_USE] && !inpt->lock[ACTIONBAR_USE]) {
		inpt->lock[ACTIONBAR_USE] = true;

		unsigned cur_slot = static_cast<unsigned>(tablist.getCurrent());

		if (cur_slot == tablist.size()-4)
			menu_c = true;
		else if (cur_slot == tablist.size()-3)
			menu_i = true;
		else if (cur_slot == tablist.size()-2)
			menu_p = true;
		else if (cur_slot == tablist.size()-1)
			menu_l = true;
	}
}

/**
 * Set all hotkeys at once e.g. when loading a game
 */
void MenuActionBar::set(std::vector<int> power_id) {
	for (unsigned int i = 0; i < slots_count; i++) {
		if (static_cast<unsigned>(power_id[i]) >= powers->powers.size())
			continue;

		hotkeys[i] = power_id[i];
	}
	updated = true;
}

void MenuActionBar::resetSlots() {
	for (unsigned int i = 0; i < slots_count; i++) {
		if (slots[i]) {
			slots[i]->checked = false;
			slots[i]->pressed = false;
		}
	}
}

/**
 * Set a target depending on how a power was triggered
 */
FPoint MenuActionBar::setTarget(bool have_aim, bool aim_assist) {
	if (have_aim && MOUSE_AIM) {
		if (aim_assist)
			return screen_to_map(inpt->mouse.x,  inpt->mouse.y + AIM_ASSIST, hero->stats.pos.x, hero->stats.pos.y);
		else
			return screen_to_map(inpt->mouse.x,  inpt->mouse.y, hero->stats.pos.x, hero->stats.pos.y);
	}
	else {
		return calcVector(hero->stats.pos, hero->stats.direction, hero->stats.melee_range);
	}
}

void MenuActionBar::setItemCount(unsigned index, int count, bool is_equipped) {
	if (index >= slots_count || !slots[index]) return;

	slot_item_count[index] = count;
	if (count == 0) {
		if (slot_activated[index])
			slots[index]->deactivate();

		slot_enabled[index] = false;
	}

	if (is_equipped)
		// we don't care how many of an equipped item we're carrying
		slots[index]->setAmount(count, 0);
	else if (count >= 0)
		// we can always carry more than 1 of any item, so always display non-equipped item count
		slots[index]->setAmount(count, 2);
	else
		// slot contains a regular power, so ignore item count
		slots[index]->setAmount(0,0);
}

bool MenuActionBar::isWithinSlots(const Point& mouse) {
	for (unsigned i=0; i<slots_count; i++) {
		if (slots[i] && isWithin(slots[i]->pos, mouse))
			return true;
	}
	return false;
}

bool MenuActionBar::isWithinMenus(const Point& mouse) {
	for (unsigned i=0; i<4; i++) {
		if (isWithin(menus[i]->pos, mouse))
			return true;
	}
	return false;
}

/**
 * Replaces the power(s) in slots that match the target_id with the power of id
 * So a target_id of 0 will place the power in an empty slot, if available
 */
void MenuActionBar::addPower(const int id, const int target_id) {
	if (static_cast<unsigned>(id) >= powers->powers.size())
		return;

	// can't put passive powers on the action bar
	if (powers->powers[id].passive)
		return;

	// if we're not replacing an existing power, avoid placing duplicate powers
	if (target_id == 0) {
		for (unsigned i=0; i<12; ++i) {
			if (hotkeys[i] == id)
				return;
		}
	}

	// MAIN slots have priority
	for (unsigned i=10; i<12; ++i) {
		if (hotkeys[i] == target_id) {
			hotkeys[i] = id;
			updated = true;
			if (target_id == 0)
				return;
		}
	}

	// now try 0-9 slots
	for (unsigned i=0; i<10; ++i) {
		if (hotkeys[i] == target_id) {
			hotkeys[i] = id;
			updated = true;
			if (target_id == 0)
				return;
		}
	}
}

Point MenuActionBar::getSlotPos(int slot) {
	if (static_cast<unsigned>(slot) < slots.size()) {
		return Point(slots[slot]->pos.x, slots[slot]->pos.y);
	}
	else if (static_cast<unsigned>(slot) < slots.size() + 4) {
		int slot_size = static_cast<int>(slots.size());
		return Point(menus[slot - slot_size]->pos.x, menus[slot - slot_size]->pos.y);
	}
	return Point();
}

MenuActionBar::~MenuActionBar() {

	menu_act = NULL;
	if (emptyslot)
		delete emptyslot;
	if (disabled)
		delete disabled;
	if (attention)
		delete attention;

	labels.clear();
	menu_labels.clear();

	for (unsigned i = 0; i < slots_count; i++)
		delete slots[i];

	for (unsigned int i=0; i<4; i++)
		delete menus[i];
}
