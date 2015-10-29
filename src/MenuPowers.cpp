/*
Copyright © 2011-2012 Clint Bellanger
Copyright © 2012 Igor Paliychuk
Copyright © 2012 Stefan Beller
Copyright © 2013-2014 Henrik Andersson

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
 * class MenuPowers
 */

#include "CommonIncludes.h"
#include "Menu.h"
#include "MenuPowers.h"
#include "Settings.h"
#include "SharedGameResources.h"
#include "SharedResources.h"
#include "StatBlock.h"
#include "UtilsParsing.h"
#include "WidgetLabel.h"
#include "WidgetSlot.h"
#include "TooltipData.h"
#include "MenuActionBar.h"

#include <climits>

MenuPowers::MenuPowers(StatBlock *_stats, MenuActionBar *_action_bar)
	: stats(_stats)
	, action_bar(_action_bar)
	, skip_section(false)
	, powers_unlock(NULL)
	, overlay_disabled(NULL)
	, points_left(0)
	, default_background("")
	, tab_control(NULL)
	, tree_loaded(false)
	, newPowerNotification(false)
{

	closeButton = new WidgetButton("images/menus/buttons/button_x.png");

	// Read powers data from config file
	FileParser infile;
	// @CLASS MenuPowers: Menu layout|Description of menus/powers.txt
	if (infile.open("menus/powers.txt")) {
		while (infile.next()) {
			if (parseMenuKey(infile.key, infile.val))
				continue;

			// @ATTR label_title|label|Position of the "Powers" text.
			if (infile.key == "label_title") title = eatLabelInfo(infile.val);
			// @ATTR unspent_points|label|Position of the text that displays the amount of unused power points.
			else if (infile.key == "unspent_points") unspent_points = eatLabelInfo(infile.val);
			// @ATTR close|x (integer), y (integer)|Position of the close button.
			else if (infile.key == "close") close_pos = toPoint(infile.val);
			// @ATTR tab_area|x (integer), y (integer), w (integer), h (integer)|Position and dimensions of the tree pages.
			else if (infile.key == "tab_area") tab_area = toRect(infile.val);

			else infile.error("MenuPowers: '%s' is not a valid key.", infile.key.c_str());
		}
		infile.close();
	}

	loadGraphics();

	menu_powers = this;

	color_bonus = font->getColor("menu_bonus");
	color_penalty = font->getColor("menu_penalty");
	color_flavor = font->getColor("item_flavor");

	align();
}

void MenuPowers::align() {
	Menu::align();

	label_powers.set(window_area.x+title.x, window_area.y+title.y, title.justify, title.valign, msg->get("Powers"), font->getColor("menu_normal"), title.font_style);

	closeButton->pos.x = window_area.x+close_pos.x;
	closeButton->pos.y = window_area.y+close_pos.y;

	stat_up.set(window_area.x+unspent_points.x, window_area.y+unspent_points.y, unspent_points.justify, unspent_points.valign, "", font->getColor("menu_bonus"), unspent_points.font_style);

	if (tab_control) {
		tab_control->setMainArea(window_area.x+tab_area.x, window_area.y+tab_area.y, tab_area.w, tab_area.h);
		tab_control->updateHeader();
	}

	for (unsigned int i=0; i<slots.size(); i++) {
		if (!slots[i]) continue;

		slots[i]->setPos(window_area.x, window_area.y);

		if (upgradeButtons[i] != NULL) {
			upgradeButtons[i]->setPos(window_area.x, window_area.y);
		}
	}

}

void MenuPowers::loadGraphics() {

	Image *graphics;

	setBackground("images/menus/powers.png");

	graphics = render_device->loadImage("images/menus/powers_unlock.png");
	if (graphics) {
		powers_unlock = graphics->createSprite();
		graphics->unref();
	}

	graphics = render_device->loadImage("images/menus/disabled.png");
	if (graphics) {
		overlay_disabled = graphics->createSprite();
		graphics->unref();
	}
}

/**
 * Loads a given power tree and sets up the menu accordingly
 */
void MenuPowers::loadPowerTree(const std::string &filename) {
	// only load the power tree once per instance
	if (tree_loaded) return;

	// First, parse the power tree file

	FileParser infile;
	// @CLASS MenuPowers: Power tree layout|Description of powers/trees/
	if (infile.open(filename)) {
		while (infile.next()) {
			if (infile.new_section) {
				// for sections that are stored in collections, add a new object here
				if (infile.section == "power") {
					slots.push_back(NULL);
					upgradeButtons.push_back(NULL);
					power_cell.push_back(Power_Menu_Cell());
				}
				else if (infile.section == "upgrade")
					power_cell_upgrade.push_back(Power_Menu_Cell());
				else if (infile.section == "tab")
					tabs.push_back(Power_Menu_Tab());
			}

			if (infile.section == "") {
				// @ATTR background|string|Filename of the default background image
				if (infile.key == "background") default_background = infile.val;
			}
			else if (infile.section == "tab")
				loadTab(infile);
			else if (infile.section == "power")
				loadPower(infile);
			else if (infile.section == "upgrade")
				loadUpgrade(infile);
		}
		infile.close();
	}

	// save a copy of the base level powers, as they get overwritten during upgrades
	power_cell_base = power_cell;

	// store the appropriate level for all upgrades
	for (unsigned i=0; i<power_cell_upgrade.size(); ++i) {
		for (unsigned j=0; j<power_cell_base.size(); j++) {
			std::vector<short>::iterator it = std::find(power_cell_base[j].upgrades.begin(), power_cell_base[j].upgrades.end(), power_cell_upgrade[i].id);
			if (it != power_cell_base[j].upgrades.end()) {
				power_cell_upgrade[i].upgrade_level = static_cast<short>(std::distance(power_cell_base[j].upgrades.begin(), it) + 2);
				break;
			}
		}
	}

	// combine base and upgrade powers into a single list
	for (unsigned i=0; i<power_cell_base.size(); ++i) {
		power_cell_all.push_back(power_cell_base[i]);
	}
	for (unsigned i=0; i<power_cell_upgrade.size(); ++i) {
		power_cell_all.push_back(power_cell_upgrade[i]);
	}

	// load any specified graphics into the tree_surf vector
	Image *graphics;
	if (tabs.empty() && default_background != "") {
		graphics = render_device->loadImage(default_background);
		if (graphics) {
			tree_surf.push_back(graphics->createSprite());
			graphics->unref();
		}
	}
	else {
		for (unsigned int i = 0; i < tabs.size(); ++i) {
			if (tabs[i].background == "")
				tabs[i].background = default_background;

			if (tabs[i].background == "") {
				tree_surf.push_back(NULL);
				continue;
			}

			graphics = render_device->loadImage(tabs[i].background);
			if (graphics) {
				tree_surf.push_back(graphics->createSprite());
				graphics->unref();
			}
			else {
				tree_surf.push_back(NULL);
			}
		}
	}

	// If we have more than one tab, create tab_control
	if (!tabs.empty()) {
		tab_control = new WidgetTabControl();

		if (tab_control) {
			// Initialize the tab control.
			tab_control->setMainArea(window_area.x+tab_area.x, window_area.y+tab_area.y, tab_area.w, tab_area.h);

			// Define the header.
			for (unsigned i=0; i<tabs.size(); i++)
				tab_control->setTabTitle(i, msg->get(tabs[i].title));
			tab_control->updateHeader();
		}
	}

	// create power slots
	for (unsigned int i=0; i<slots.size(); i++) {
		if (static_cast<unsigned>(power_cell[i].id) < powers->powers.size()) {
			slots[i] = new WidgetSlot(powers->powers[power_cell[i].id].icon);
			slots[i]->setBasePos(power_cell[i].pos.x, power_cell[i].pos.y);
			tablist.add(slots[i]);

			if (upgradeButtons[i] != NULL) {
				upgradeButtons[i]->setBasePos(power_cell[i].pos.x + ICON_SIZE, power_cell[i].pos.y);
			}
		}
	}

	applyPowerUpgrades();

	tree_loaded = true;

	align();
}

short MenuPowers::id_by_powerIndex(short power_index, const std::vector<Power_Menu_Cell>& cell) {
	// Find cell with our power
	for (unsigned i=0; i<cell.size(); i++)
		if (cell[i].id == power_index)
			return static_cast<short>(i);

	return -1;
}

/**
 * Apply power upgrades on savegame loading
 */
void MenuPowers::applyPowerUpgrades() {
	for (unsigned i = 0; i < power_cell.size(); i++) {
		if (!power_cell[i].upgrades.empty()) {
			std::vector<short>::iterator it;
			for (it = power_cell[i].upgrades.end(); it != power_cell[i].upgrades.begin(); ) {
				--it;
				std::vector<int>::iterator upgrade_it;
				upgrade_it = std::find(stats->powers_list.begin(), stats->powers_list.end(), *it);
				if (upgrade_it != stats->powers_list.end()) {
					short upgrade_index = id_by_powerIndex(static_cast<short>(*upgrade_it), power_cell_upgrade);
					if (upgrade_index != -1)
						replacePowerCellDataByUpgrade(static_cast<short>(i), upgrade_index);
					break;
				}
			}
		}
	}
	setUnlockedPowers();
}

/**
 * Find cell in upgrades with next upgrade for current power_cell
 */
short MenuPowers::nextLevel(short power_cell_index) {
	if (power_cell[power_cell_index].upgrades.empty()) {
		return -1;
	}

	std::vector<short>::iterator level_it;
	level_it = std::find(power_cell[power_cell_index].upgrades.begin(),
					power_cell[power_cell_index].upgrades.end(),
					power_cell[power_cell_index].id);

	if (level_it == power_cell[power_cell_index].upgrades.end()) {
		// current power is base power, take first upgrade
		short index = power_cell[power_cell_index].upgrades[0];
		return id_by_powerIndex(index, power_cell_upgrade);
	}
	// current power is an upgrade, take next upgrade if avaliable
	short index = static_cast<short>(std::distance(power_cell[power_cell_index].upgrades.begin(), level_it));
	if (static_cast<short>(power_cell[power_cell_index].upgrades.size()) > index + 1) {
		return id_by_powerIndex(*(++level_it), power_cell_upgrade);
	}
	else {
		return -1;
	}
}

/**
 * Replace data in power_cell[cell_index] by data in upgrades
 */
void MenuPowers::upgradePower(short power_cell_index) {
	short i = nextLevel(power_cell_index);
	if (i == -1)
		return;

	// if power was present in ActionBar, update it there
	action_bar->addPower(power_cell_upgrade[i].id, power_cell[power_cell_index].id);

	// if we have tab_control
	if (tab_control) {
		int active_tab = tab_control->getActiveTab();
		if (power_cell[power_cell_index].tab == active_tab) {
			replacePowerCellDataByUpgrade(power_cell_index, i);
			stats->powers_list.push_back(power_cell_upgrade[i].id);
			stats->check_title = true;
		}
	}
	// if have don't have tabs
	else {
		replacePowerCellDataByUpgrade(power_cell_index, i);
		stats->powers_list.push_back(power_cell_upgrade[i].id);
		stats->check_title = true;
	}
	setUnlockedPowers();
}

void MenuPowers::replacePowerCellDataByUpgrade(short power_cell_index, short upgrade_cell_index) {
	power_cell[power_cell_index].id = power_cell_upgrade[upgrade_cell_index].id;
	power_cell[power_cell_index].requires_physoff = power_cell_upgrade[upgrade_cell_index].requires_physoff;
	power_cell[power_cell_index].requires_physdef = power_cell_upgrade[upgrade_cell_index].requires_physdef;
	power_cell[power_cell_index].requires_mentoff = power_cell_upgrade[upgrade_cell_index].requires_mentoff;
	power_cell[power_cell_index].requires_mentdef = power_cell_upgrade[upgrade_cell_index].requires_mentdef;
	power_cell[power_cell_index].requires_defense = power_cell_upgrade[upgrade_cell_index].requires_defense;
	power_cell[power_cell_index].requires_offense = power_cell_upgrade[upgrade_cell_index].requires_offense;
	power_cell[power_cell_index].requires_physical = power_cell_upgrade[upgrade_cell_index].requires_physical;
	power_cell[power_cell_index].requires_mental = power_cell_upgrade[upgrade_cell_index].requires_mental;
	power_cell[power_cell_index].requires_level = power_cell_upgrade[upgrade_cell_index].requires_level;
	power_cell[power_cell_index].requires_power = power_cell_upgrade[upgrade_cell_index].requires_power;
	power_cell[power_cell_index].requires_point = power_cell_upgrade[upgrade_cell_index].requires_point;
	power_cell[power_cell_index].passive_on = power_cell_upgrade[upgrade_cell_index].passive_on;
	power_cell[power_cell_index].upgrade_level = power_cell_upgrade[upgrade_cell_index].upgrade_level;

	if (slots[power_cell_index])
		slots[power_cell_index]->setIcon(powers->powers[power_cell_upgrade[upgrade_cell_index].id].icon);
}

bool MenuPowers::baseRequirementsMet(int power_index) {
	int id = id_by_powerIndex(static_cast<short>(power_index), power_cell_all);

	if (id == -1)
		return false;

	for (unsigned i = 0; i < power_cell_all[id].requires_power.size(); ++i)
		if (!requirementsMet(power_cell_all[id].requires_power[i]))
			return false;

	if ((stats->physoff() >= power_cell_all[id].requires_physoff) &&
			(stats->physdef() >= power_cell_all[id].requires_physdef) &&
			(stats->mentoff() >= power_cell_all[id].requires_mentoff) &&
			(stats->mentdef() >= power_cell_all[id].requires_mentdef) &&
			(stats->get_defense() >= power_cell_all[id].requires_defense) &&
			(stats->get_offense() >= power_cell_all[id].requires_offense) &&
			(stats->get_physical() >= power_cell_all[id].requires_physical) &&
			(stats->get_mental() >= power_cell_all[id].requires_mental) &&
			(stats->level >= power_cell_all[id].requires_level)) return true;
	return false;
}

/**
 * With great power comes great stat requirements.
 */
bool MenuPowers::requirementsMet(int power_index) {

	// power_index can be 0 during recursive call if requires_power is not defined.
	// Power with index 0 doesn't exist and is always enabled
	if (power_index == 0) return true;

	int id = id_by_powerIndex(static_cast<short>(power_index), power_cell_all);

	// If we didn't find power in power_menu, than it has no requirements
	if (id == -1) return true;

	if (!powerIsVisible(static_cast<short>(power_index))) return false;

	// If power_id is saved into vector, it's unlocked anyway
	// check power_cell_unlocked and stats->powers_list
	for (unsigned i=0; i<power_cell_unlocked.size(); ++i) {
		if (power_cell_unlocked[i].id == power_index)
			return true;
	}
	if (std::find(stats->powers_list.begin(), stats->powers_list.end(), power_index) != stats->powers_list.end()) return true;

	// Check the rest requirements
	if (baseRequirementsMet(power_index) && !power_cell_all[id].requires_point) return true;
	return false;
}

/**
 * Check if we can unlock power.
 */
bool MenuPowers::powerUnlockable(int power_index) {

	// power_index can be 0 during recursive call if requires_power is not defined.
	// Power with index 0 doesn't exist and is always enabled
	if (power_index == 0) return true;

	// Find cell with our power
	int id = id_by_powerIndex(static_cast<short>(power_index), power_cell_all);

	// If we didn't find power in power_menu, than it has no requirements
	if (id == -1) return true;

	if (!powerIsVisible(static_cast<short>(power_index))) return false;

	// If we already have a power, don't try to unlock it
	if (requirementsMet(power_index)) return false;

	// Check requirements
	if (baseRequirementsMet(power_index)) return true;
	return false;
}

/**
 * Click-to-drag a power (to the action bar)
 */
int MenuPowers::click(Point mouse) {
	int active_tab = (tab_control) ? tab_control->getActiveTab() : 0;

	for (unsigned i=0; i<power_cell.size(); i++) {
		if (slots[i] && isWithin(slots[i]->pos, mouse) && (power_cell[i].tab == active_tab)) {
			if (TOUCHSCREEN) {
				if (!slots[i]->in_focus) {
					slots[i]->in_focus = true;
					tablist.setCurrent(slots[i]);
					return 0;
				}
			}

			if (powerUnlockable(power_cell[i].id) && points_left > 0 && power_cell[i].requires_point) {
				// unlock power
				stats->powers_list.push_back(power_cell[i].id);
				stats->check_title = true;
				setUnlockedPowers();
				action_bar->addPower(power_cell[i].id, 0);
				return 0;
			}
			else if (requirementsMet(power_cell[i].id) && !powers->powers[power_cell[i].id].passive) {
				// pick up and drag power
				slots[i]->in_focus = false;
				return power_cell[i].id;
			}
			else
				return 0;
		}
	}
	return 0;
}

short MenuPowers::getPointsUsed() {
	short used = 0;
	for (unsigned i=0; i<power_cell_unlocked.size(); ++i) {
		if (power_cell_unlocked[i].requires_point)
			used++;
	}

	return used;
}

void MenuPowers::setUnlockedPowers() {
	std::vector<int> power_ids;
	power_cell_unlocked.clear();

	for (unsigned i=0; i<power_cell.size(); ++i) {
		if (std::find(stats->powers_list.begin(), stats->powers_list.end(), power_cell[i].id) != stats->powers_list.end()) {
			// base power
			if (std::find(power_ids.begin(), power_ids.end(), power_cell[i].id) == power_ids.end()) {
				power_ids.push_back(power_cell_base[i].id);
				power_cell_unlocked.push_back(power_cell_base[i]);
			}
			if (power_cell[i].id == power_cell_base[i].id)
				continue;

			//upgrades
			for (unsigned j=0; j<power_cell[i].upgrades.size(); ++j) {
				if (std::find(power_ids.begin(), power_ids.end(), power_cell[i].upgrades[j]) == power_ids.end()) {
					int id = id_by_powerIndex(power_cell[i].upgrades[j], power_cell_upgrade);
					if (id != -1) {
						power_ids.push_back(power_cell[i].upgrades[j]);
						power_cell_unlocked.push_back(power_cell_upgrade[id]);

						if (power_cell[i].id == power_cell[i].upgrades[j])
							break;
					}
					else {
						break;
					}
				}
			}
		}
	}
}

void MenuPowers::logic() {
	for (unsigned i=0; i<power_cell_unlocked.size(); i++) {
		if (static_cast<unsigned>(power_cell_unlocked[i].id) < powers->powers.size() && powers->powers[power_cell_unlocked[i].id].passive) {
			bool unlocked_power = std::find(stats->powers_list.begin(), stats->powers_list.end(), power_cell_unlocked[i].id) != stats->powers_list.end();
			std::vector<int>::iterator it = std::find(stats->powers_passive.begin(), stats->powers_passive.end(), power_cell_unlocked[i].id);

			if (it != stats->powers_passive.end()) {
				if (!baseRequirementsMet(power_cell_unlocked[i].id) && power_cell_unlocked[i].passive_on) {
					stats->powers_passive.erase(it);
					stats->effects.removeEffectPassive(power_cell_unlocked[i].id);
					power_cell[i].passive_on = false;
					stats->refresh_stats = true;
				}
			}
			else if (((baseRequirementsMet(power_cell_unlocked[i].id) && !power_cell_unlocked[i].requires_point) || unlocked_power) && !power_cell_unlocked[i].passive_on) {
				stats->powers_passive.push_back(power_cell_unlocked[i].id);
				power_cell_unlocked[i].passive_on = true;
				// for passives without special triggers, we need to trigger them here
				if (stats->effects.triggered_others)
					powers->activateSinglePassive(stats, power_cell_unlocked[i].id);
			}
		}
	}

	for (unsigned i=0; i<power_cell.size(); i++) {
		//upgrade buttons logic
		if (upgradeButtons[i] != NULL) {
			upgradeButtons[i]->enabled = false;
			// enable button only if current level is unlocked and next level can be unlocked
			if (canUpgrade(static_cast<short>(i))) {
				upgradeButtons[i]->enabled = true;
			}
			if ((!tab_control || power_cell[i].tab == tab_control->getActiveTab()) && upgradeButtons[i]->checkClick()) {
				upgradePower(static_cast<short>(i));
			}
		}
	}

	points_left = static_cast<short>((stats->level * stats->power_points_per_level) - getPointsUsed());
	if (points_left > 0) {
		newPowerNotification = true;
	}

	if (!visible) return;

	tablist.logic();

	if (closeButton->checkClick()) {
		visible = false;
		snd->play(sfx_close);
	}

	if (tab_control) {
		// make shure keyboard navigation leads us to correct tab
		for (unsigned int i = 0; i < slots.size(); i++) {
			if (slots[i] && slots[i]->in_focus)
				tab_control->setActiveTab(power_cell[i].tab);
		}

		tab_control->logic();
	}
}

bool MenuPowers::canUpgrade(short power_cell_index) {
	return (nextLevel(power_cell_index) != -1 &&
			requirementsMet(power_cell[power_cell_index].id) &&
			powerUnlockable(power_cell_upgrade[nextLevel(power_cell_index)].id) &&
			points_left > 0 &&
			power_cell_upgrade[nextLevel(power_cell_index)].requires_point);
}

void MenuPowers::render() {
	if (!visible) return;

	Rect src;
	Rect dest;

	// background
	dest = window_area;
	src.x = 0;
	src.y = 0;
	src.w = window_area.w;
	src.h = window_area.h;

	setBackgroundClip(src);
	setBackgroundDest(dest);
	Menu::render();


	if (tab_control) {
		tab_control->render();
		unsigned active_tab = tab_control->getActiveTab();
		for (unsigned i=0; i<tabs.size(); i++) {
			if (active_tab == i) {
				// power tree
				Sprite *r = tree_surf[i];
				if (r) {
					r->setClip(src);
					r->setDest(dest);
					render_device->render(r);
				}

				// power icons
				renderPowers(active_tab);
			}
		}
	}
	else if (!tree_surf.empty()) {
		Sprite *r = tree_surf[0];
		if (r) {
			r->setClip(src);
			r->setDest(dest);
			render_device->render(r);
		}
		renderPowers(0);
	}
	else {
		renderPowers(0);
	}

	// close button
	closeButton->render();

	// text overlay
	if (!title.hidden) label_powers.render();

	// stats
	if (!unspent_points.hidden) {
		std::stringstream ss;

		ss.str("");
		if (points_left !=0) {
			ss << msg->get("Unspent skill points:") << " " << points_left;
		}
		stat_up.set(ss.str());
		stat_up.render();
	}
}

/**
 * Highlight unlocked powers
 */
void MenuPowers::displayBuild(int power_id) {
	Rect src_unlock;

	src_unlock.x = 0;
	src_unlock.y = 0;
	src_unlock.w = ICON_SIZE;
	src_unlock.h = ICON_SIZE;

	for (unsigned i=0; i<power_cell.size(); i++) {
		if (power_cell[i].id == power_id && powers_unlock && slots[i]) {
			powers_unlock->setClip(src_unlock);
			powers_unlock->setDest(slots[i]->pos);
			render_device->render(powers_unlock);
		}
	}
}

/**
 * Show mouseover descriptions of disciplines and powers
 */
TooltipData MenuPowers::checkTooltip(Point mouse) {

	TooltipData tip;

	for (unsigned i=0; i<power_cell.size(); i++) {

		if (tab_control && (tab_control->getActiveTab() != power_cell[i].tab)) continue;

		if (!powerIsVisible(power_cell[i].id)) continue;

		if (slots[i] && isWithin(slots[i]->pos, mouse)) {
			bool base_unlocked = requirementsMet(power_cell[i].id) || std::find(stats->powers_list.begin(), stats->powers_list.end(), power_cell[i].id) != stats->powers_list.end();

			generatePowerDescription(&tip, i, power_cell, !base_unlocked);
			if (!power_cell[i].upgrades.empty()) {
				short next_level = nextLevel(static_cast<short>(i));
				if (next_level != -1) {
					tip.addText("\n" + msg->get("Next Level:"));
					generatePowerDescription(&tip, next_level, power_cell_upgrade, base_unlocked);
				}
			}

			return tip;
		}
	}

	return tip;
}

void MenuPowers::generatePowerDescription(TooltipData* tip, int slot_num, const std::vector<Power_Menu_Cell>& power_cells, bool show_unlock_prompt) {
	if (power_cells[slot_num].upgrade_level > 0)
		tip->addText(powers->powers[power_cells[slot_num].id].name + " (" + msg->get("Level %d", power_cells[slot_num].upgrade_level) + ")");
	else
		tip->addText(powers->powers[power_cells[slot_num].id].name);

	if (powers->powers[power_cells[slot_num].id].passive) tip->addText("Passive");
	tip->addText(powers->powers[power_cells[slot_num].id].description, color_flavor);

	// add mana cost
	if (powers->powers[power_cells[slot_num].id].requires_mp > 0) {
		tip->addText(msg->get("Costs %d MP", powers->powers[power_cells[slot_num].id].requires_mp));
	}
	// add health cost
	if (powers->powers[power_cells[slot_num].id].requires_hp > 0) {
		tip->addText(msg->get("Costs %d HP", powers->powers[power_cells[slot_num].id].requires_hp));
	}
	// add cooldown time
	if (powers->powers[power_cells[slot_num].id].cooldown > 0) {
		std::stringstream ss;
		ss << msg->get("Cooldown:") << " " << getDurationString(powers->powers[power_cells[slot_num].id].cooldown);
		tip->addText(ss.str());
	}

	const Power &pwr = powers->powers[power_cells[slot_num].id];
	for (size_t i=0; i<pwr.post_effects.size(); ++i) {
		std::stringstream ss;
		EffectDef* effect_ptr = powers->getEffectDef(pwr.post_effects[i].id);

		// base stats
		if (effect_ptr == NULL) {
			if (pwr.post_effects[i].magnitude > 0) {
				ss << "+";
			}

			if (pwr.passive) {
				// since leveled passive powers stack, we need to get the total magnitude for all levels
				// TODO: don't stack leveled passive powers?
				ss << pwr.post_effects[i].magnitude * power_cells[slot_num].upgrade_level;
			}
			else {
				ss << pwr.post_effects[i].magnitude;
			}

			for (size_t j=0; j<STAT_COUNT; ++j) {
				if (pwr.post_effects[i].id == STAT_KEY[j]) {
					if (STAT_PERCENT[j])
						ss << "%";

					ss << " " << STAT_NAME[j];

					break;
				}
			}

			for (size_t j=0; j<ELEMENTS.size(); ++j) {
				if (pwr.post_effects[i].id == ELEMENTS[j].id + "_resist") {
					ss << "% " << msg->get(ELEMENTS[j].name);
					break;
				}
			}
		}
		else if (effect_ptr != NULL) {
			if (effect_ptr->type == "damage") {
				ss << pwr.post_effects[i].magnitude << " " << msg->get("Damage per second");
			}
			else if (effect_ptr->type == "damage_percent") {
				ss << pwr.post_effects[i].magnitude << "% " << msg->get("Damage per second");
			}
			else if (effect_ptr->type == "hpot") {
				ss << pwr.post_effects[i].magnitude << " " << msg->get("HP per second");
			}
			else if (effect_ptr->type == "hpot_percent") {
				ss << pwr.post_effects[i].magnitude << "% " << msg->get("HP per second");
			}
			else if (effect_ptr->type == "mpot") {
				ss << pwr.post_effects[i].magnitude << " " << msg->get("MP per second");
			}
			else if (effect_ptr->type == "mpot_percent") {
				ss << pwr.post_effects[i].magnitude << "% " << msg->get("MP per second");
			}
			else if (effect_ptr->type == "speed") {
				if (pwr.post_effects[i].magnitude == 0)
					ss << msg->get("Immobilize");
				else
					ss << msg->get("%d%% Speed", pwr.post_effects[i].magnitude);
			}
			else if (effect_ptr->type == "immunity") {
				ss << msg->get("Immunity");
			}
			else if (effect_ptr->type == "stun") {
				ss << msg->get("Stun");
			}
			else if (effect_ptr->type == "revive") {
				ss << msg->get("Automatic revive on death");
			}
			else if (effect_ptr->type == "convert") {
				ss << msg->get("Convert");
			}
			else if (effect_ptr->type == "fear") {
				ss << msg->get("Fear");
			}
			else if (effect_ptr->type == "offense") {
				ss << pwr.post_effects[i].magnitude << " " << msg->get("Offense");
			}
			else if (effect_ptr->type == "defense") {
				ss << pwr.post_effects[i].magnitude << " " << msg->get("Defense");
			}
			else if (effect_ptr->type == "physical") {
				ss << pwr.post_effects[i].magnitude << " " << msg->get("Physical");
			}
			else if (effect_ptr->type == "mental") {
				ss << pwr.post_effects[i].magnitude << " " << msg->get("Mental");
			}
			else if (effect_ptr->type == "death_sentence") {
				ss << msg->get("Lifespan");
			}
			else if (effect_ptr->type == "shield") {
				if (pwr.mod_damage_mode == STAT_MODIFIER_MODE_MULTIPLY) {
					int magnitude = stats->get(STAT_DMG_MENT_MAX) * pwr.mod_damage_value_min / 100;
					ss << magnitude;
				}
				else if (pwr.mod_damage_mode == STAT_MODIFIER_MODE_ADD) {
					int magnitude = stats->get(STAT_DMG_MENT_MAX) + pwr.mod_damage_value_min;
					ss << magnitude;
				}
				else if (pwr.mod_damage_mode == STAT_MODIFIER_MODE_ABSOLUTE) {
					if (pwr.mod_damage_value_max == 0 || pwr.mod_damage_value_min == pwr.mod_damage_value_max)
						ss << pwr.mod_damage_value_min;
					else
						ss << pwr.mod_damage_value_min << "-" << pwr.mod_damage_value_max;
				}
				else {
					ss << stats->get(STAT_DMG_MENT_MAX);
				}

				ss << " " << msg->get("Magical Shield");
			}
			else if (effect_ptr->type == "heal") {
				int mag_min = stats->get(STAT_DMG_MENT_MIN);
				int mag_max = stats->get(STAT_DMG_MENT_MAX);

				if (pwr.mod_damage_mode == STAT_MODIFIER_MODE_MULTIPLY) {
					mag_min = mag_min * pwr.mod_damage_value_min / 100;
					mag_max = mag_max * pwr.mod_damage_value_min / 100;
					ss << mag_min << "-" << mag_max;
				}
				else if (pwr.mod_damage_mode == STAT_MODIFIER_MODE_ADD) {
					mag_min = mag_min + pwr.mod_damage_value_min;
					mag_max = mag_max + pwr.mod_damage_value_min;
					ss << mag_min << "-" << mag_max;
				}
				else if (pwr.mod_damage_mode == STAT_MODIFIER_MODE_ABSOLUTE) {
					if (pwr.mod_damage_value_max == 0 || pwr.mod_damage_value_min == pwr.mod_damage_value_max)
						ss << pwr.mod_damage_value_min;
					else
						ss << pwr.mod_damage_value_min << "-" << pwr.mod_damage_value_max;
				}
				else {
					ss << mag_min << "-" << mag_max;
				}

				ss << " " << msg->get("Healing");
			}
			else if (effect_ptr->type == "knockback") {
				ss << pwr.post_effects[i].magnitude << " " << msg->get("Knockback");
			}
			else if (pwr.post_effects[i].magnitude == 0) {
				// nothing
			}
		}
		else {
			ss << pwr.post_effects[i].id;
		}

		if (!ss.str().empty()) {
			if (pwr.post_effects[i].duration > 0) {
				if (effect_ptr && effect_ptr->type == "death_sentence") {
					ss << ": " << getDurationString(pwr.post_effects[i].duration);
				}
				else {
					ss << " (" << getDurationString(pwr.post_effects[i].duration) << ")";
				}
			}

			tip->addText(ss.str(), color_bonus);
		}
	}

	if (pwr.use_hazard || pwr.type == POWTYPE_REPEATER) {
		std::stringstream ss;

		// modifier_damage
		if (pwr.mod_damage_mode > -1) {
			if (pwr.mod_damage_mode == STAT_MODIFIER_MODE_ADD && pwr.mod_damage_value_min > 0)
				ss << "+";

			if (pwr.mod_damage_value_max == 0 || pwr.mod_damage_value_min == pwr.mod_damage_value_max) {
				ss << pwr.mod_damage_value_min;
			}
			else {
				ss << pwr.mod_damage_value_min << "-" << pwr.mod_damage_value_max;
			}

			if (pwr.mod_damage_mode == STAT_MODIFIER_MODE_MULTIPLY) {
				ss << "%";
			}
			ss << " ";

			if (pwr.base_damage == BASE_DAMAGE_NONE)
				ss << msg->get("Damage");
			else if (pwr.base_damage == BASE_DAMAGE_MELEE)
				ss << msg->get("Melee Damage");
			else if (pwr.base_damage == BASE_DAMAGE_RANGED)
				ss << msg->get("Ranged Damage");
			else if (pwr.base_damage == BASE_DAMAGE_MENT)
				ss << msg->get("Mental Damage");

			if (!ss.str().empty())
				tip->addText(ss.str(), color_bonus);
		}

		// modifier_accuracy
		if (pwr.mod_accuracy_mode > -1) {
			ss.str("");

			if (pwr.mod_accuracy_mode == STAT_MODIFIER_MODE_ADD && pwr.mod_accuracy_value > 0)
				ss << "+";

			ss << pwr.mod_accuracy_value;

			if (pwr.mod_accuracy_mode == STAT_MODIFIER_MODE_MULTIPLY) {
				ss << "%";
			}
			ss << " ";

			ss << msg->get("Base Accuracy");

			if (!ss.str().empty())
				tip->addText(ss.str(), color_bonus);
		}

		// modifier_critical
		if (pwr.mod_crit_mode > -1) {
			ss.str("");

			if (pwr.mod_crit_mode == STAT_MODIFIER_MODE_ADD && pwr.mod_crit_value > 0)
				ss << "+";

			ss << pwr.mod_crit_value;

			if (pwr.mod_crit_mode == STAT_MODIFIER_MODE_MULTIPLY) {
				ss << "%";
			}
			ss << " ";

			ss << msg->get("Base Critical Chance");

			if (!ss.str().empty())
				tip->addText(ss.str(), color_bonus);
		}

		if (pwr.trait_armor_penetration) {
			ss.str("");
			ss << msg->get("Ignores Absorbtion");
			tip->addText(ss.str(), color_bonus);
		}
		if (pwr.trait_avoidance_ignore) {
			ss.str("");
			ss << msg->get("Ignores Avoidance");
			tip->addText(ss.str(), color_bonus);
		}
		if (pwr.trait_crits_impaired > 0) {
			ss.str("");
			ss << msg->get("%d%% Chance to crit slowed targets", pwr.trait_crits_impaired);
			tip->addText(ss.str(), color_bonus);
		}
		if (pwr.trait_elemental > -1) {
			ss.str("");
			// TODO Print specific element here
			ss << msg->get("%s Elemental Damage", ELEMENTS[pwr.trait_elemental].name.c_str());
			tip->addText(ss.str(), color_bonus);
		}
	}

	std::set<std::string>::iterator it;
	for (it = powers->powers[power_cells[slot_num].id].requires_flags.begin(); it != powers->powers[power_cells[slot_num].id].requires_flags.end(); ++it) {
		for (unsigned i=0; i<EQUIP_FLAGS.size(); ++i) {
			if ((*it) == EQUIP_FLAGS[i].id) {
				tip->addText(msg->get("Requires a %s", msg->get(EQUIP_FLAGS[i].name)));
			}
		}
	}

	// add requirement
	if ((power_cells[slot_num].requires_physoff > 0) && (stats->physoff() < power_cells[slot_num].requires_physoff)) {
		tip->addText(msg->get("Requires Physical Offense %d", power_cells[slot_num].requires_physoff), color_penalty);
	}
	else if((power_cells[slot_num].requires_physoff > 0) && (stats->physoff() >= power_cells[slot_num].requires_physoff)) {
		tip->addText(msg->get("Requires Physical Offense %d", power_cells[slot_num].requires_physoff));
	}
	if ((power_cells[slot_num].requires_physdef > 0) && (stats->physdef() < power_cells[slot_num].requires_physdef)) {
		tip->addText(msg->get("Requires Physical Defense %d", power_cells[slot_num].requires_physdef), color_penalty);
	}
	else if ((power_cells[slot_num].requires_physdef > 0) && (stats->physdef() >= power_cells[slot_num].requires_physdef)) {
		tip->addText(msg->get("Requires Physical Defense %d", power_cells[slot_num].requires_physdef));
	}
	if ((power_cells[slot_num].requires_mentoff > 0) && (stats->mentoff() < power_cells[slot_num].requires_mentoff)) {
		tip->addText(msg->get("Requires Mental Offense %d", power_cells[slot_num].requires_mentoff), color_penalty);
	}
	else if ((power_cells[slot_num].requires_mentoff > 0) && (stats->mentoff() >= power_cells[slot_num].requires_mentoff)) {
		tip->addText(msg->get("Requires Mental Offense %d", power_cells[slot_num].requires_mentoff));
	}
	if ((power_cells[slot_num].requires_mentdef > 0) && (stats->mentdef() < power_cells[slot_num].requires_mentdef)) {
		tip->addText(msg->get("Requires Mental Defense %d", power_cells[slot_num].requires_mentdef), color_penalty);
	}
	else if ((power_cells[slot_num].requires_mentdef > 0) && (stats->mentdef() >= power_cells[slot_num].requires_mentdef)) {
		tip->addText(msg->get("Requires Mental Defense %d", power_cells[slot_num].requires_mentdef));
	}
	if ((power_cells[slot_num].requires_offense > 0) && (stats->get_offense() < power_cells[slot_num].requires_offense)) {
		tip->addText(msg->get("Requires Offense %d", power_cells[slot_num].requires_offense), color_penalty);
	}
	else if ((power_cells[slot_num].requires_offense > 0) && (stats->get_offense() >= power_cells[slot_num].requires_offense)) {
		tip->addText(msg->get("Requires Offense %d", power_cells[slot_num].requires_offense));
	}
	if ((power_cells[slot_num].requires_defense > 0) && (stats->get_defense() < power_cells[slot_num].requires_defense)) {
		tip->addText(msg->get("Requires Defense %d", power_cells[slot_num].requires_defense), color_penalty);
	}
	else if ((power_cells[slot_num].requires_defense > 0) && (stats->get_defense() >= power_cells[slot_num].requires_defense)) {
		tip->addText(msg->get("Requires Defense %d", power_cells[slot_num].requires_defense));
	}
	if ((power_cells[slot_num].requires_physical > 0) && (stats->get_physical() < power_cells[slot_num].requires_physical)) {
		tip->addText(msg->get("Requires Physical %d", power_cells[slot_num].requires_physical), color_penalty);
	}
	else if ((power_cells[slot_num].requires_physical > 0) && (stats->get_physical() >= power_cells[slot_num].requires_physical)) {
		tip->addText(msg->get("Requires Physical %d", power_cells[slot_num].requires_physical));
	}
	if ((power_cells[slot_num].requires_mental > 0) && (stats->get_mental() < power_cells[slot_num].requires_mental)) {
		tip->addText(msg->get("Requires Mental %d", power_cells[slot_num].requires_mental), color_penalty);
	}
	else if ((power_cells[slot_num].requires_mental > 0) && (stats->get_mental() >= power_cells[slot_num].requires_mental)) {
		tip->addText(msg->get("Requires Mental %d", power_cells[slot_num].requires_mental));
	}

	// Draw required Level Tooltip
	if ((power_cells[slot_num].requires_level > 0) && stats->level < power_cells[slot_num].requires_level) {
		tip->addText(msg->get("Requires Level %d", power_cells[slot_num].requires_level), color_penalty);
	}
	else if ((power_cells[slot_num].requires_level > 0) && stats->level >= power_cells[slot_num].requires_level) {
		tip->addText(msg->get("Requires Level %d", power_cells[slot_num].requires_level));
	}

	for (unsigned j = 0; j < power_cells[slot_num].requires_power.size(); ++j) {
		if (power_cells[slot_num].requires_power[j] == 0) continue;

		short req_index = id_by_powerIndex(power_cells[slot_num].requires_power[j], power_cell_all);
		if (req_index == -1) continue;

		std::string req_power_name;
		if (power_cell_all[req_index].upgrade_level > 0)
			req_power_name = powers->powers[power_cell_all[req_index].id].name + " (" + msg->get("Level %d", power_cell_all[req_index].upgrade_level) + ")";
		else
			req_power_name = powers->powers[power_cell_all[req_index].id].name;


		// Required Power Tooltip
		if (!(requirementsMet(power_cells[slot_num].requires_power[j]))) {
			tip->addText(msg->get("Requires Power: %s", req_power_name), color_penalty);
		}
		else {
			tip->addText(msg->get("Requires Power: %s", req_power_name));
		}

	}

	// Draw unlock power Tooltip
	if (power_cells[slot_num].requires_point && !(std::find(stats->powers_list.begin(), stats->powers_list.end(), power_cells[slot_num].id) != stats->powers_list.end())) {
		if (show_unlock_prompt && points_left > 0 && powerUnlockable(power_cells[slot_num].id)) {
			tip->addText(msg->get("Click to Unlock (uses 1 Skill Point)"), color_bonus);
		}
		else {
			if (power_cells[slot_num].requires_point && points_left < 1)
				tip->addText(msg->get("Requires 1 Skill Point"), color_penalty);
			else
				tip->addText(msg->get("Requires 1 Skill Point"));
		}
	}
}

MenuPowers::~MenuPowers() {
	if (powers_unlock) delete powers_unlock;
	if (overlay_disabled) delete overlay_disabled;

	for (unsigned int i=0; i<tree_surf.size(); i++) {
		if (tree_surf[i]) delete tree_surf[i];
	}
	for (unsigned int i=0; i<slots.size(); i++) {
		delete slots.at(i);
		delete upgradeButtons.at(i);
	}
	slots.clear();
	upgradeButtons.clear();

	delete closeButton;
	if (tab_control) delete tab_control;
	menu_powers = NULL;
}

/**
 * Return true if required stats for power usage are met. Else return false.
 */
bool MenuPowers::meetsUsageStats(unsigned powerid) {

	// Find cell with our power
	int id = id_by_powerIndex(static_cast<short>(powerid), power_cell);
	// If we didn't find power in power_menu, than it has no stats requirements
	if (id == -1) return true;

	return stats->physoff() >= power_cell[id].requires_physoff
		   && stats->physdef() >= power_cell[id].requires_physdef
		   && stats->mentoff() >= power_cell[id].requires_mentoff
		   && stats->mentdef() >= power_cell[id].requires_mentdef
		   && stats->get_defense() >= power_cell[id].requires_defense
		   && stats->get_offense() >= power_cell[id].requires_offense
		   && stats->get_mental() >= power_cell[id].requires_mental
		   && stats->get_physical() >= power_cell[id].requires_physical;
}

void MenuPowers::renderPowers(int tab_num) {

	Rect disabled_src;
	disabled_src.x = disabled_src.y = 0;
	disabled_src.w = disabled_src.h = ICON_SIZE;

	for (unsigned i=0; i<power_cell.size(); i++) {
		bool power_in_vector = false;

		// Continue if slot is not filled with data
		if (power_cell[i].tab != tab_num) continue;

		if (!powerIsVisible(power_cell[i].id)) continue;

		if (std::find(stats->powers_list.begin(), stats->powers_list.end(), power_cell[i].id) != stats->powers_list.end()) power_in_vector = true;

		if (slots[i])
			slots[i]->render();

		// highlighting
		if (power_in_vector || requirementsMet(power_cell[i].id)) {
			displayBuild(power_cell[i].id);
		}
		else {
			if (overlay_disabled && slots[i]) {
				overlay_disabled->setClip(disabled_src);
				overlay_disabled->setDest(slots[i]->pos);
				render_device->render(overlay_disabled);
			}
		}

		if (slots[i])
			slots[i]->renderSelection();

		// upgrade buttons
		if (upgradeButtons[i])
			upgradeButtons[i]->render();
	}
}

bool MenuPowers::powerIsVisible(short power_index) {

	// power_index can be 0 during recursive call if requires_power is not defined.
	// Power with index 0 doesn't exist and is always enabled
	if (power_index == 0) return true;

	// Find cell with our power
	int id = id_by_powerIndex(power_index, power_cell_all);

	// If we didn't find power in power_menu, than it has no requirements
	if (id == -1) return true;

	for (unsigned i = 0; i < power_cell_all[id].visible_requires_status.size(); ++i)
		if (!camp->checkStatus(power_cell_all[id].visible_requires_status[i]))
			return false;

	for (unsigned i = 0; i < power_cell_all[id].visible_requires_not.size(); ++i)
		if (camp->checkStatus(power_cell_all[id].visible_requires_not[i]))
			return false;

	return true;
}

/**
 * Below are functions used for parsing the power trees located in:
 * powers/trees/
 */

void MenuPowers::loadTab(FileParser &infile) {
	// @ATTR tab.title|string|The name of this power tree tab
	if (infile.key == "title") tabs.back().title = infile.val;
	// @ATTR tab.background|string|Filename of the background image for this tab's power tree
	else if (infile.key == "background") tabs.back().background = infile.val;
}

void MenuPowers::loadPower(FileParser &infile) {
	// @ATTR power.id|integer|A power id from powers/powers.txt for this slot.
	if (infile.key == "id") {
		int id = popFirstInt(infile.val);
		if (id > 0) {
			skip_section = false;
			power_cell.back().id = static_cast<short>(id);
		}
		else {
			infile.error("MenuPowers: Power index out of bounds 1-%d, skipping power.", INT_MAX);
		}
		return;
	}

	if (power_cell.back().id <= 0) {
		skip_section = true;
		power_cell.pop_back();
		slots.pop_back();
		upgradeButtons.pop_back();
		logError("MenuPowers: There is a power without a valid id as the first attribute. IDs must be the first attribute in the power menu definition.");
	}

	if (skip_section)
		return;

	// @ATTR power.tab|integer|Tab index to place this power on, starting from 0.
	if (infile.key == "tab") power_cell.back().tab = static_cast<short>(toInt(infile.val));
	// @ATTR power.position|x (integer), y (integer)|Position of this power icon; relative to MenuPowers "pos".
	else if (infile.key == "position") power_cell.back().pos = toPoint(infile.val);

	// @ATTR power.requires_physoff|integer|Power requires Physical and Offense stat of this value.
	else if (infile.key == "requires_physoff") power_cell.back().requires_physoff = static_cast<short>(toInt(infile.val));
	// @ATTR power.requires_physdef|integer|Power requires Physical and Defense stat of this value.
	else if (infile.key == "requires_physdef") power_cell.back().requires_physdef = static_cast<short>(toInt(infile.val));
	// @ATTR power.requires_mentoff|integer|Power requires Mental and Offense stat of this value.
	else if (infile.key == "requires_mentoff") power_cell.back().requires_mentoff = static_cast<short>(toInt(infile.val));
	// @ATTR power.requires_mentdef|integer|Power requires Mental and Defense stat of this value.
	else if (infile.key == "requires_mentdef") power_cell.back().requires_mentdef = static_cast<short>(toInt(infile.val));

	// @ATTR power.requires_defense|integer|Power requires Defense stat of this value.
	else if (infile.key == "requires_defense") power_cell.back().requires_defense = static_cast<short>(toInt(infile.val));
	// @ATTR power.requires_offense|integer|Power requires Offense stat of this value.
	else if (infile.key == "requires_offense") power_cell.back().requires_offense = static_cast<short>(toInt(infile.val));
	// @ATTR power.requires_physical|integer|Power requires Physical stat of this value.
	else if (infile.key == "requires_physical") power_cell.back().requires_physical = static_cast<short>(toInt(infile.val));
	// @ATTR power.requires_mental|integer|Power requires Mental stat of this value.
	else if (infile.key == "requires_mental") power_cell.back().requires_mental = static_cast<short>(toInt(infile.val));

	// @ATTR power.requires_point|boolean|Power requires a power point to unlock.
	else if (infile.key == "requires_point") power_cell.back().requires_point = toBool(infile.val);
	// @ATTR power.requires_level|integer|Power requires at least this level for the hero.
	else if (infile.key == "requires_level") power_cell.back().requires_level = static_cast<short>(toInt(infile.val));
	// @ATTR power.requires_power|integer|Power requires another power id.
	else if (infile.key == "requires_power") power_cell.back().requires_power.push_back(static_cast<short>(toInt(infile.val)));

	// @ATTR power.visible_requires_status|string|Hide the power if we don't have this campaign status.
	else if (infile.key == "visible_requires_status") power_cell.back().visible_requires_status.push_back(infile.val);
	// @ATTR power.visible_requires_not_status|string|Hide the power if we have this campaign status.
	else if (infile.key == "visible_requires_not_status") power_cell.back().visible_requires_not.push_back(infile.val);

	// @ATTR power.upgrades|id (integer), ...|A list of upgrade power ids that this power slot can upgrade to. Each of these powers should have a matching upgrade section.
	else if (infile.key == "upgrades") {
		upgradeButtons.back() = new WidgetButton("images/menus/buttons/button_plus.png");
		std::string repeat_val = infile.nextValue();
		while (repeat_val != "") {
			power_cell.back().upgrades.push_back(static_cast<short>(toInt(repeat_val)));
			repeat_val = infile.nextValue();
		}

		if (!power_cell.back().upgrades.empty())
			power_cell.back().upgrade_level = 1;
	}

	else infile.error("MenuPowers: '%s' is not a valid key.", infile.key.c_str());
}

void MenuPowers::loadUpgrade(FileParser &infile) {
	// @ATTR upgrade.id|integer|A power id from powers/powers.txt for this upgrade.
	if (infile.key == "id") {
		int id = popFirstInt(infile.val);
		if (id > 0) {
			skip_section = false;
			power_cell_upgrade.back().id = static_cast<short>(id);
		}
		else {
			skip_section = true;
			power_cell_upgrade.pop_back();
			infile.error("MenuPowers: Power index out of bounds 1-%d, skipping power.", INT_MAX);
		}
		return;
	}

	if (skip_section)
		return;

	// @ATTR upgrade.requires_physoff|integer|Upgrade requires Physical and Offense stat of this value.
	if (infile.key == "requires_physoff") power_cell_upgrade.back().requires_physoff = static_cast<short>(toInt(infile.val));
	// @ATTR upgrade.requires_physdef|integer|Upgrade requires Physical and Defense stat of this value.
	else if (infile.key == "requires_physdef") power_cell_upgrade.back().requires_physdef = static_cast<short>(toInt(infile.val));
	// @ATTR upgrade.requires_mentoff|integer|Upgrade requires Mental and Offense stat of this value.
	else if (infile.key == "requires_mentoff") power_cell_upgrade.back().requires_mentoff = static_cast<short>(toInt(infile.val));
	// @ATTR upgrade.requires_mentdef|integer|Upgrade requires Mental and Defense stat of this value.
	else if (infile.key == "requires_mentdef") power_cell_upgrade.back().requires_mentdef = static_cast<short>(toInt(infile.val));

	// @ATTR upgrade.requires_defense|integer|Upgrade requires Defense stat of this value.
	else if (infile.key == "requires_defense") power_cell_upgrade.back().requires_defense = static_cast<short>(toInt(infile.val));
	// @ATTR upgrade.requires_offense|integer|Upgrade requires Offense stat of this value.
	else if (infile.key == "requires_offense") power_cell_upgrade.back().requires_offense = static_cast<short>(toInt(infile.val));
	// @ATTR upgrade.requires_physical|integer|Upgrade requires Physical stat of this value.
	else if (infile.key == "requires_physical") power_cell_upgrade.back().requires_physical = static_cast<short>(toInt(infile.val));
	// @ATTR upgrade.requires_mental|integer|Upgrade requires Mental stat of this value.
	else if (infile.key == "requires_mental") power_cell_upgrade.back().requires_mental = static_cast<short>(toInt(infile.val));

	// @ATTR upgrade.requires_point|boolean|Upgrade requires a power point to unlock.
	else if (infile.key == "requires_point") power_cell_upgrade.back().requires_point = toBool(infile.val);
	// @ATTR upgrade.requires_level|integer|Upgrade requires at least this level for the hero.
	else if (infile.key == "requires_level") power_cell_upgrade.back().requires_level = static_cast<short>(toInt(infile.val));
	// @ATTR upgrade.requires_power|integer|Upgrade requires another power id.
	else if (infile.key == "requires_power") power_cell_upgrade.back().requires_power.push_back(static_cast<short>(toInt(infile.val)));

	// @ATTR upgrade.visible_requires_status|string|Hide the upgrade if we don't have this campaign status.
	else if (infile.key == "visible_requires_status") power_cell_upgrade.back().visible_requires_status.push_back(infile.val);
	// @ATTR upgrade.visible_requires_not_status|string|Hide the upgrade if we have this campaign status.
	else if (infile.key == "visible_requires_not_status") power_cell_upgrade.back().visible_requires_not.push_back(infile.val);

	else infile.error("MenuPowers: '%s' is not a valid key.", infile.key.c_str());
}

void MenuPowers::resetToBasePowers() {
	for (unsigned i=0; i<power_cell.size(); ++i) {
		power_cell[i] = power_cell_base[i];
	}
}
