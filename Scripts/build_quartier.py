"""
Construit la carte /Game/Bodycam/Maps/L_Quartier, ambiance fin de journée :
 - rue : asphalte, flaques, trottoirs, marquages, plaques d'égout, voitures garées + épave brûlée,
   lampadaires allumés (un qui clignote), câbles, abribus, panneaux, bouches d'incendie, poubelles,
 - immeubles : fenêtres (certaines éclairées), appuis, corniches, clims, gouttières, balcons,
 - ruelle en cul-de-sac : escalier de secours, benne, sacs poubelle, applique en panne,
 - commerce abandonné enterable : enseigne néon qui grésille, néon intérieur qui clignote,
 - cour industrielle + entrepôt abandonné enterable : néons défaillants, chariot élévateur,
   rayonnages, mezzanine-bureau, débris, flaques sous le toit percé, poussière.
Éclairage mixte : soleil + ciel stationnaires, lampadaires statiques (précalculés), néons mobiles.
Relançable : la carte est recréée à chaque exécution.
"""
import math
import os
import random
import sys

import unreal

sys.path.append(os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Scripts"))
import bodycam_materials  # noqa: E402

MAP_DIR = "/Game/Bodycam/Maps"
MAP_PATH = MAP_DIR + "/L_Quartier"
random.seed(7)

M = bodycam_materials.get_surfaces()
E = bodycam_materials.get_emissives()
CUBE = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")
CYLINDER = unreal.load_asset("/Engine/BasicShapes/Cylinder.Cylinder")
SPHERE = unreal.load_asset("/Engine/BasicShapes/Sphere.Sphere")
CONE = unreal.load_asset("/Game/StarterContent/Shapes/Shape_Cone.Shape_Cone")
FLICKER_CLASS = unreal.load_class(None, "/Script/BodycamUE.BodycamFlickerLight")
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
COUNT = [0]


# ------------------------------------------------------------------ outils

def _lightmap_res(size):
    biggest = max(size)
    if biggest <= 150:
        return 32
    if biggest <= 500:
        return 64
    if biggest <= 1000:
        return 128
    return 256


def shape(label, mesh_asset, center, size, mat, folder, yaw=0.0, pitch=0.0, roll=0.0, shadow=True):
    """Forme de base (cube/cylindre/sphère de 100 cm centrés) mise à l'échelle size (cm)."""
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*center),
                                          unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw))
    comp = actor.static_mesh_component
    comp.set_static_mesh(mesh_asset)
    comp.set_material(0, mat)
    comp.set_editor_property("override_light_map_res", True)
    comp.set_editor_property("overridden_light_map_res", _lightmap_res(size))
    if not shadow:
        comp.set_editor_property("cast_shadow", False)
    actor.set_actor_scale3d(unreal.Vector(size[0] / 100.0, size[1] / 100.0, size[2] / 100.0))
    actor.set_actor_label(label)
    actor.set_folder_path(folder)
    COUNT[0] += 1
    return actor


def prop(label, center, size, mat, folder="Props", yaw=0.0, pitch=0.0, roll=0.0, shadow=True):
    return shape(label, CUBE, center, size, mat, folder, yaw, pitch, roll, shadow)


def cylinder(label, base, diameter, height, mat, folder="Props"):
    return shape(label, CYLINDER, (base[0], base[1], base[2] + height / 2), (diameter, diameter, height), mat, folder)


def block(label, x0, x1, y0, y1, z0, z1, mat, folder="Divers", max_piece=1200.0):
    """Bloc aligné sur les axes, découpé en morceaux (lightmaps plus nettes)."""
    nx = max(1, int(math.ceil((x1 - x0) / max_piece)))
    ny = max(1, int(math.ceil((y1 - y0) / max_piece)))
    dx, dy = (x1 - x0) / nx, (y1 - y0) / ny
    for i in range(nx):
        for j in range(ny):
            ax, ay = x0 + i * dx, y0 + j * dy
            prop(label, (ax + dx / 2, ay + dy / 2, (z0 + z1) / 2), (dx, dy, z1 - z0), mat, folder)


def mesh(label, path, loc, yaw=0.0, scale=(1, 1, 1), mat=None, folder="Props"):
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*loc), unreal.Rotator(yaw=yaw))
    actor.static_mesh_component.set_static_mesh(unreal.load_asset(path))
    if mat:
        actor.static_mesh_component.set_material(0, mat)
    actor.set_actor_scale3d(unreal.Vector(*scale))
    actor.set_actor_label(label)
    actor.set_folder_path(folder)
    COUNT[0] += 1
    return actor


def point_light(label, loc, candela, radius, color, mobility=unreal.ComponentMobility.STATIC, folder="Lumieres"):
    light = actors.spawn_actor_from_class(unreal.PointLight, unreal.Vector(*loc))
    comp = light.point_light_component
    light.root_component.set_mobility(mobility)
    comp.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
    comp.set_editor_property("intensity", candela)
    comp.set_editor_property("attenuation_radius", radius)
    comp.set_light_color(unreal.LinearColor(color[0], color[1], color[2], 1.0))
    light.set_actor_label(label)
    light.set_folder_path(folder)
    return light


def flicker(label, loc, pattern, fixture_size, fixture_mat, candela, color, radius, emissive, folder="Lumieres/Neons",
            yaw=0.0, light_offset=-25.0, speed=1.0):
    """Lampe qui clignote (C++ BodycamFlickerLight) : lumière + tube émissif synchronisés."""
    actor = actors.spawn_actor_from_class(FLICKER_CLASS, unreal.Vector(*loc), unreal.Rotator(yaw=yaw))
    actor.set_editor_property("pattern", getattr(unreal.BodycamFlicker, pattern))
    actor.set_editor_property("light_intensity", candela)
    actor.set_editor_property("emissive_intensity", emissive)
    actor.set_editor_property("speed", speed)
    fixture = actor.get_editor_property("fixture")
    fixture.set_static_mesh(CUBE)
    fixture.set_material(0, fixture_mat)
    fixture.set_relative_scale3d(unreal.Vector(fixture_size[0] / 100.0, fixture_size[1] / 100.0, fixture_size[2] / 100.0))
    light = actor.get_editor_property("light")
    light.set_light_color(unreal.LinearColor(color[0], color[1], color[2], 1.0))
    light.set_editor_property("attenuation_radius", radius)
    light.set_relative_location(unreal.Vector(0, 0, light_offset), False, True)
    actor.set_actor_label(label)
    actor.set_folder_path(folder)
    return actor


def window_material():
    r = random.random()
    if r < 0.22:
        return E["WindowWarm"]
    if r < 0.30:
        return E["WindowCold"]
    return M["Glass"]


def windows_on_y_face(label, x0, x1, y_face, outward, z0, z1, floor_h=300, spacing=360, w=140, h=170, folder="Facades"):
    """Fenêtres + appuis (+ quelques clims) sur une façade perpendiculaire à Y (outward = -1 ou +1)."""
    count = int((x1 - x0 - 120) // spacing)
    start = x0 + ((x1 - x0) - (count - 1) * spacing) / 2.0
    z = z0
    while z + floor_h <= z1:
        bottom = z + 90
        for i in range(count):
            xc = start + i * spacing
            ya, yb = sorted((y_face, y_face + outward * 8))
            prop(label + "_Fenetre", (xc, (ya + yb) / 2, bottom + h / 2), (w, 8, h), window_material(), folder, shadow=False)
            ya, yb = sorted((y_face, y_face + outward * 16))
            prop(label + "_Appui", (xc, (ya + yb) / 2, bottom - 5), (w + 24, 16, 10), M["ConcreteLight"], folder)
            if random.random() < 0.15:
                ya, yb = sorted((y_face, y_face + outward * 50))
                prop(label + "_Clim", (xc + 40, (ya + yb) / 2, bottom - 45), (75, 50, 55), M["CarGrey"], folder)
        z += floor_h


def windows_on_x_face(label, y0, y1, x_face, outward, z0, z1, floor_h=300, spacing=360, w=140, h=170, folder="Facades"):
    count = int((y1 - y0 - 120) // spacing)
    start = y0 + ((y1 - y0) - (count - 1) * spacing) / 2.0
    z = z0
    while z + floor_h <= z1:
        bottom = z + 90
        for i in range(count):
            yc = start + i * spacing
            xa, xb = sorted((x_face, x_face + outward * 8))
            prop(label + "_Fenetre", ((xa + xb) / 2, yc, bottom + h / 2), (8, w, h), window_material(), folder, shadow=False)
        z += floor_h


def cornice_y(label, x0, x1, y_face, outward, z):
    ya, yb = sorted((y_face, y_face + outward * 25))
    block(label + "_Corniche", x0, x1, ya, yb, z - 35, z, M["ConcreteLight"], "Facades")


def drainpipe(label, x, y, height):
    cylinder(label + "_Gouttiere", (x, y, 0), 11, height, M["SteelDark"], "Facades")


def crate(label, x, y, z=0.0, size=100.0, yaw=0.0):
    prop(label, (x, y, z + size / 2), (size, size, size), M["Wood"], "Props/Caisses", yaw=yaw)


def pallet(label, x, y, z=0.0, yaw=0.0):
    prop(label, (x, y, z + 7), (120, 100, 14), M["Wood"], "Props/Palettes", yaw=yaw)


def debris(label, cx, cy, radius, count, mat, z=0.0):
    for _ in range(count):
        s = random.uniform(15, 60)
        prop(label, (cx + random.uniform(-radius, radius), cy + random.uniform(-radius, radius), z + s * 0.3),
             (s * random.uniform(0.8, 2.0), s, s * random.uniform(0.4, 0.9)), mat, "Props/Debris",
             yaw=random.uniform(0, 360), pitch=random.uniform(-25, 25), roll=random.uniform(-25, 25), shadow=False)


def papers(cx, cy, radius, count, z=0.0):
    for _ in range(count):
        prop("Papier", (cx + random.uniform(-radius, radius), cy + random.uniform(-radius, radius), z + 0.4),
             (21, 30, 0.4), M["Paper"], "Props/Debris", yaw=random.uniform(0, 360), shadow=False)


def puddle(cx, cy, z=0.0, size=250.0):
    for _ in range(random.randint(2, 3)):
        prop("Flaque", (cx + random.uniform(-size * 0.3, size * 0.3), cy + random.uniform(-size * 0.3, size * 0.3), z + 0.25),
             (size * random.uniform(0.5, 1.0), size * random.uniform(0.3, 0.6), 0.5), M["Puddle"], "Props/Flaques",
             yaw=random.uniform(0, 180), shadow=False)


def trash_bags(cx, cy, count):
    for _ in range(count):
        w = random.uniform(45, 65)
        shape("Sac_Poubelle", SPHERE, (cx + random.uniform(-80, 80), cy + random.uniform(-60, 60), w * 0.35),
              (w, w * 0.85, w * 0.75), M["TrashBag"], "Props/Dechets", yaw=random.uniform(0, 360))


def car(label, x, y, yaw, paint, burnt=False):
    """Voiture simplifiée (caisse, habitacle vitré, roues, pare-chocs, phares)."""
    folder = "Rue/Voitures"
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))

    def at(dx, dy, z):
        return (x + dx * c - dy * s, y + dx * s + dy * c, z)

    roll = 4.0 if burnt else 0.0
    body = M["Charred"] if burnt else M[paint]
    prop(label + "_Caisse", at(0, 0, 62), (440, 178, 62), body, folder, yaw=yaw, roll=roll)
    prop(label + "_Habitacle", at(-25, 0, 122), (230, 160, 58), M["Charred"] if burnt else M["Glass"], folder, yaw=yaw, roll=roll)
    prop(label + "_Toit", at(-30, 0, 153), (200, 152, 6), body, folder, yaw=yaw, roll=roll)
    prop(label + "_ParechocAv", at(224, 0, 45), (10, 180, 22), M["Rubber"], folder, yaw=yaw)
    prop(label + "_ParechocAr", at(-224, 0, 45), (10, 180, 22), M["Rubber"], folder, yaw=yaw)
    for dx in (-140, 140):
        for dy in (-82, 82):
            shape(label + "_Roue", CYLINDER, at(dx, dy, 31), (62, 62, 22), M["Rubber"], folder, yaw=yaw, roll=90)
    if not burnt:
        for dy in (-62, 62):
            prop(label + "_Phare", at(222, dy, 72), (6, 32, 14), E["Headlight"], folder, yaw=yaw, shadow=False)
            prop(label + "_Feu", at(-222, dy, 76), (6, 30, 12), M["PaintRed"], folder, yaw=yaw, shadow=False)


# ------------------------------------------------------------------ carte

unreal.EditorAssetLibrary.make_directory(MAP_DIR)
for path in (MAP_PATH, MAP_PATH + "_BuiltData"):
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)

X0, X1 = -3600, 3600
T = 30

# --- Rue -----------------------------------------------------------------
block("Route", X0, X1, -500, 500, -40, 0, M["Asphalt"], "Rue")
block("Trottoir_N", X0, X1, 500, 900, -40, 15, M["Sidewalk"], "Rue")
block("Trottoir_S", X0, X1, -1000, -500, -40, 15, M["Sidewalk"], "Rue")
block("Bordure_N", X0, X1, 500, 515, -40, 16, M["ConcreteLight"], "Rue")
block("Bordure_S", X0, X1, -515, -500, -40, 16, M["ConcreteLight"], "Rue")
for x in range(X0 + 200, X1 - 300, 700):
    prop("Marquage", (x + 175, 0, 0.3), (350, 15, 0.6), M["Paint"], "Rue/Marquages", shadow=False)
for x in range(-300, 300, 100):
    prop("Passage_Pieton", (x + 25, 0, 0.3), (50, 900, 0.6), M["Paint"], "Rue/Marquages", shadow=False)
for x in (X0 + 300, X1 - 300):
    prop("Ligne_Stop", (x, 0, 0.3), (40, 1000, 0.6), M["Paint"], "Rue/Marquages", shadow=False)
block("Bordure_Jaune", 900, 1900, -515, -500, 16, 17, M["PaintYellow"], "Rue/Marquages")
for x in (-2600, -800, 900, 2700):
    cylinder("Plaque_Egout", (x, random.choice((-250, 230)), 0), 70, 1, M["SteelDark"], "Rue")
for (x, y) in ((-2900, -350), (-1500, 280), (200, -330), (1900, 360), (3000, -200)):
    puddle(x, y)

# Voitures garées le long des trottoirs + épave brûlée au milieu
car("Voiture_1", -2700, 390, 180, "CarWhite")
car("Voiture_2", -2050, 395, 181, "CarGrey")
car("Voiture_3", 2300, 390, 179, "CarBlue")
car("Voiture_4", -900, -390, 0, "CarRed")
car("Voiture_5", 2900, -395, 1, "CarGrey")
car("Epave", 1450, -60, 24, "CarGrey", burnt=True)
debris("Epave_Debris", 1450, -60, 280, 10, M["Charred"])
for (x, y) in ((1050, -300), (1150, 250), (1900, -280), (1800, 330)):
    shape("Cone", CONE, (x, y, 0), (35, 35, 70), M["PaintRed"], "Rue/Mobilier")

# Lampadaires allumés (statiques, précalculés) ; l'un d'eux grésille
for side, y in ((1, 830), (-1, -830)):
    for i, x in enumerate(range(X0 + 500, X1, 1400)):
        cylinder("Lampadaire_Mat", (x, y, 15), 14, 620, M["SteelDark"], "Rue/Lampadaires")
        prop("Lampadaire_Bras", (x, y - side * 90, 625), (12, 190, 10), M["SteelDark"], "Rue/Lampadaires")
        head = (x, y - side * 185, 615)
        if side == 1 and i == 2:
            flicker("Lampadaire_Defaillant", head, "FLICKER", (35, 55, 18), E["StreetLamp"], 300,
                    (1.0, 0.6, 0.28), 1600, 25, folder="Rue/Lampadaires", light_offset=-40)
        else:
            prop("Lampadaire_Tete", head, (35, 55, 18), E["StreetLamp"], "Rue/Lampadaires", shadow=False)
            point_light("Lampadaire_Lumiere", (head[0], head[1], 575), 300, 1600, (1.0, 0.6, 0.28))
    for x in range(X0 + 1200, X1, 2100):
        cylinder("Poubelle", (x, y, 15), 55, 95, M["SteelDark"], "Rue/Mobilier")
        trash_bags(x + 90, y + side * 10, 2)
# Câbles électriques au-dessus de la rue
for x in range(X0 + 500, X1, 1400):
    prop("Cable", (x, 0, 605), (2, 1660, 2), M["Rubber"], "Rue/Cables", shadow=False)

# Bouches d'incendie et panneaux
for (x, y) in ((-3000, 600), (600, 600), (-2200, -600), (2400, -600)):
    cylinder("Borne_Incendie", (x, y, 15), 24, 62, M["PaintRed"], "Rue/Mobilier")
    shape("Borne_Chapeau", SPHERE, (x, y, 80), (26, 26, 16), M["PaintRed"], "Rue/Mobilier")
for (x, y) in ((-1250, 600), (1300, -620), (3300, 600)):
    cylinder("Panneau_Mat", (x, y, 15), 6, 260, M["SteelDark"], "Rue/Mobilier")
    prop("Panneau", (x, y - 4, 245), (60, 4, 60), M["SignBlue"], "Rue/Mobilier")

# Abribus côté sud
SX = -1700
for dx in (-200, 200):
    for y in (-960, -760):
        prop("Abribus_Poteau", (SX + dx, y, 140), (8, 8, 250), M["SteelDark"], "Rue/Abribus")
prop("Abribus_Toit", (SX, -860, 268), (430, 230, 8), M["SteelDark"], "Rue/Abribus")
prop("Abribus_Vitre_Fond", (SX, -965, 140), (400, 4, 210), M["Glass"], "Rue/Abribus")
prop("Abribus_Banc", (SX, -930, 60), (300, 40, 8), M["Wood"], "Rue/Abribus")
prop("Abribus_Pub", (SX + 210, -860, 140), (6, 120, 180), E["WindowCold"], "Rue/Abribus", shadow=False)

# Fermeture des extrémités de la rue
for x_end, sign in ((X0, 1), (X1, -1)):
    block("Mur_Fin", min(x_end, x_end - sign * 60), max(x_end, x_end - sign * 60), -1000, 900, 0, 450,
          M["ConcreteDirty"], "Limites")
    for y in range(-450, 451, 220):
        prop("Barriere_Beton", (x_end + sign * 150, y, 45), (60, 200, 90), M["ConcreteLight"], "Rue/Mobilier")

# --- Immeubles côté nord (façade sur y = 900) ------------------------------
block("B1", X0, -1400, 900, 2700, 0, 1300, M["BrickRed"], "Immeubles/B1")
windows_on_y_face("B1", X0 + 100, -1400, 900, -1, 400, 1300)
cornice_y("B1", X0, -1400, 900, -1, 1300)
cornice_y("B1_RdC", X0, -1400, 900, -1, 400)
windows_on_x_face("B1_Ruelle", 1000, 2600, -1400, 1, 400, 1300)
drainpipe("B1", -1430, 885, 1300)
for i, x in enumerate((-3100, -2500, -1900)):
    prop("B1_Vitrine", (x, 896, 160), (300, 8, 220), E["WindowWarm"] if i == 2 else M["Glass"], "Immeubles/B1", shadow=False)
    if i == 1:
        prop("B1_Rideau_Metal", (x, 890, 160), (310, 6, 230), M["SteelDark"], "Immeubles/B1")
trash_bags(-2200, 820, 3)

# Ruelle en cul-de-sac entre B1 et B2
block("Ruelle_Sol", -1400, -1100, 900, 2700, -40, 2, M["ConcreteDirty"], "Ruelle")
block("Ruelle_Fond", -1400, -1100, 2700, 2740, 0, 650, M["BrickRed"], "Ruelle")
prop("Benne", (-1250, 2450, 70), (170, 110, 140), M["ContainerBlue"], "Ruelle")
for i in range(3):
    crate("Ruelle_Caisse", -1330 + i * 20, 2050 - i * 130, yaw=random.uniform(-20, 20))
trash_bags(-1220, 2250, 5)
debris("Ruelle_Debris", -1250, 1700, 100, 6, M["Cardboard"])
puddle(-1250, 1450, 2, 180)
flicker("Ruelle_Applique", (-1120, 2300, 320), "FAULTY", (20, 30, 18), E["StreetLamp"], 40, (1.0, 0.7, 0.4), 800, 20,
        folder="Ruelle")
# escalier de secours sur le mur de B1 (côté ruelle)
for z in (400, 700, 1000):
    prop("Secours_Palier", (-1350, 2200, z), (90, 260, 6), M["SteelDark"], "Ruelle/Secours")
    prop("Secours_Garde_Corps", (-1310, 2200, z + 95), (4, 260, 4), M["SteelDark"], "Ruelle/Secours")
    for dy in (-125, 125):
        prop("Secours_Montant", (-1310, 2200 + dy, z + 48), (4, 4, 95), M["SteelDark"], "Ruelle/Secours")
for z0 in (400, 700):
    prop("Secours_Echelle", (-1330, 2120, z0 + 150), (4, 50, 340), M["SteelDark"], "Ruelle/Secours", roll=-18)

# B2 : immeuble béton, rez-de-chaussée = commerce abandonné enterable
B2X0, B2X1 = -1100, 1300
block("Commerce_Facade", B2X0, -900, 900, 930, 0, 430, M["ConcretePanels"], "Immeubles/B2")
block("Commerce_Facade", -300, -100, 900, 930, 0, 430, M["ConcretePanels"], "Immeubles/B2")
block("Commerce_Facade", 100, 300, 900, 930, 0, 430, M["ConcretePanels"], "Immeubles/B2")
block("Commerce_Facade", 900, B2X1, 900, 930, 0, 430, M["ConcretePanels"], "Immeubles/B2")
block("Commerce_Allege", -900, -300, 900, 930, 0, 90, M["ConcretePanels"], "Immeubles/B2")
block("Commerce_Allege", 300, 900, 900, 930, 0, 90, M["ConcretePanels"], "Immeubles/B2")
block("Commerce_Linteau", -900, 900, 900, 930, 300, 430, M["ConcretePanels"], "Immeubles/B2")
block("Commerce_Murs", B2X0, B2X0 + T, 930, 1930, 0, 430, M["Plaster"], "Immeubles/B2")
block("Commerce_Murs", B2X1 - T, B2X1, 930, 1930, 0, 430, M["Plaster"], "Immeubles/B2")
block("Commerce_Fond", B2X0, B2X1, 1900, 1930, 0, 430, M["Plaster"], "Immeubles/B2")
block("Commerce_Plafond", B2X0, B2X1, 900, 1930, 400, 430, M["Plaster"], "Immeubles/B2")
block("Commerce_Sol", B2X0 + T, B2X1 - T, 930, 1900, -40, 2, M["FloorTiles"], "Immeubles/B2")
block("B2_Arriere", B2X0, B2X1, 1930, 2700, 0, 430, M["ConcretePanels"], "Immeubles/B2")
block("B2_Etages", B2X0, B2X1, 900, 2700, 430, 1500, M["ConcretePanels"], "Immeubles/B2")
windows_on_y_face("B2", B2X0, B2X1, 900, -1, 430, 1500)
cornice_y("B2", B2X0, B2X1, 900, -1, 1500)
windows_on_x_face("B2_Ruelle", 1000, 2600, B2X0, -1, 430, 1500)
drainpipe("B2", B2X1 - 20, 885, 1500)
# balcons
for (x, z) in ((-560, 730), (580, 730), (-560, 1030), (220, 1330)):
    prop("Balcon_Dalle", (x, 840, z), (280, 120, 12), M["ConcreteLight"], "Immeubles/B2/Balcons")
    prop("Balcon_Rambarde", (x, 782, z + 95), (280, 4, 4), M["SteelDark"], "Immeubles/B2/Balcons")
    for dx in (-138, -46, 46, 138):
        prop("Balcon_Barreau", (x + dx, 782, z + 50), (4, 4, 90), M["SteelDark"], "Immeubles/B2/Balcons")
# enseigne du commerce : néons rouge (qui clignote) et bleu (qui grésille)
prop("Enseigne", (0, 886, 350), (1400, 14, 90), M["SteelDark"], "Immeubles/B2/Enseigne")
flicker("Enseigne_Neon_Rouge", (0, 876, 388), "FLICKER", (1300, 5, 5), E["NeonRed"], 15, (1.0, 0.1, 0.1), 600, 25,
        folder="Immeubles/B2/Enseigne", light_offset=-10, speed=1.3)
flicker("Enseigne_Neon_Bleu", (0, 876, 312), "BUZZ", (1300, 5, 5), E["NeonBlue"], 12, (0.2, 0.4, 1.0), 600, 25,
        folder="Immeubles/B2/Enseigne", light_offset=10)
# intérieur du commerce
prop("Comptoir", (700, 1500, 55), (60, 600, 110), M["Wood"], "Immeubles/B2/Interieur")
prop("Caisse_Enregistreuse", (700, 1350, 125), (40, 45, 30), M["CarGrey"], "Immeubles/B2/Interieur")
for x in (-800, -400, 0):
    prop("Etagere", (x, 1750, 100), (300, 50, 200), M["SteelDark"], "Immeubles/B2/Interieur")
    for k in range(5):
        prop("Produit", (x - 120 + k * 60, 1730, 205 + random.uniform(0, 5)), (25, 18, random.uniform(15, 30)),
             random.choice((M["PaintRed"], M["PaintYellow"], M["SignBlue"], M["Paper"])), "Immeubles/B2/Interieur",
             yaw=random.uniform(-15, 15), shadow=False)
prop("Etagere_Renversee", (-300, 1300, 30), (300, 200, 50), M["SteelDark"], "Immeubles/B2/Interieur", yaw=25)
for _ in range(14):
    prop("Produit_Sol", (random.uniform(-900, 400), random.uniform(1050, 1650), 6), (22, 16, 12),
         random.choice((M["PaintRed"], M["PaintYellow"], M["SignBlue"], M["Paper"])), "Immeubles/B2/Interieur",
         yaw=random.uniform(0, 360), roll=random.choice((0, 90)), shadow=False)
debris("Commerce_Debris", -600, 1100, 250, 8, M["Cardboard"])
debris("Vitrine_Eclats", -600, 960, 200, 6, M["Glass"])
papers(-200, 1300, 400, 15, 2)
flicker("Commerce_Neon", (100, 1400, 392), "FLICKER", (120, 12, 6), E["Tube"], 25, (0.85, 0.95, 1.0), 900, 15,
        folder="Immeubles/B2/Interieur")

# B3 : pierre beige
block("B3", B2X1, X1, 900, 2700, 0, 1000, M["BrickBeige"], "Immeubles/B3")
windows_on_y_face("B3", B2X1 + 100, X1 - 100, 900, -1, 400, 1000)
cornice_y("B3", B2X1, X1, 900, -1, 1000)
cornice_y("B3_RdC", B2X1, X1, 900, -1, 400)
drainpipe("B3", B2X1 + 20, 885, 1000)
for x in (1800, 2500, 3200):
    prop("B3_Porte", (x, 895, 120), (130, 10, 240), M["SteelDark"], "Immeubles/B3")
    prop("B3_Marche", (x, 870, 22), (180, 50, 14), M["ConcreteLight"], "Immeubles/B3")
flicker("B3_Applique", (2500, 880, 265), "BUZZ", (40, 14, 10), E["Tube"], 20, (0.85, 0.95, 1.0), 600, 12,
        folder="Immeubles/B3")

# --- Côté sud : enceinte industrielle --------------------------------------
YW = -1000
block("Enceinte_N", X0, -400, YW - 30, YW, 0, 260, M["ConcreteDirty"], "Enceinte")
block("Enceinte_N", 400, X1, YW - 30, YW, 0, 260, M["ConcreteDirty"], "Enceinte")
block("Enceinte_O", X0, X0 + 30, -4300, YW, 0, 260, M["ConcreteDirty"], "Enceinte")
block("Enceinte_E", X1 - 30, X1, -4300, YW, 0, 260, M["ConcreteDirty"], "Enceinte")
block("Enceinte_S", X0, X1, -4330, -4300, 0, 260, M["ConcreteDirty"], "Enceinte")
prop("Portail_Ouvert", (600, YW - 60, 110), (400, 10, 220), M["MetalRust"], "Enceinte")
prop("Portail_Ouvert", (-560, YW - 220, 110), (400, 10, 220), M["MetalRust"], "Enceinte", yaw=-60)
block("Cour_Sol", X0, X1, -4300, YW, -40, 0, M["ConcreteDirty"], "Cour")
block("Cour_Gravier", -3400, -1900, -3800, -2000, -40, 1, M["Gravel"], "Cour")
point_light("Cour_Projecteur", (0, -1830, 630), 150, 1800, (1.0, 0.85, 0.65))
prop("Cour_Projecteur_Boitier", (0, -1880, 660), (40, 30, 25), E["Headlight"], "Cour", shadow=False)

prop("Conteneur", (2700, -1500, 130), (600, 245, 260), M["ContainerBlue"], "Cour")
prop("Conteneur", (2700, -1800, 130), (600, 245, 260), M["ContainerRed"], "Cour")
prop("Conteneur", (2700, -1650, 390), (600, 245, 260), M["ContainerBlue"], "Cour", yaw=4)
prop("Conteneur", (-2800, -1450, 130), (600, 245, 260), M["ContainerRed"], "Cour", yaw=90)
prop("Benne_Cour", (1300, -1250, 70), (200, 120, 140), M["ContainerBlue"], "Cour")
trash_bags(1300, -1420, 4)
for (x, y) in ((-1700, -1300), (-1550, -1350), (2000, -2300), (2120, -2350), (-3000, -3500)):
    cylinder("Fut", (x, y, 0), 60, 90, M["MetalRust"], "Cour")
for k in range(4):
    cylinder("Pneu", (-2300, -2400, k * 25), 70, 25, M["Rubber"], "Cour")
for i in range(4):
    pallet("Palette", -2000, -1350, i * 14)
    pallet("Palette", 1700, -1500, i * 14, yaw=12)
crate("Caisse_Cour", -1500, -1600)
crate("Caisse_Cour", -1500, -1600, 100, yaw=15)
crate("Caisse_Cour", -1390, -1610, size=80)
debris("Cour_Debris", -2600, -3000, 400, 10, M["ConcreteDirty"])
puddle(-1000, -1500, 0, 300)
puddle(1800, -3300, 0, 260)
car("Voiture_Cour", -2600, -2700, 70, "CarWhite")
# chariot élévateur devant l'entrepôt
prop("Chariot_Corps", (-700, -1600, 75), (220, 110, 110), M["Forklift"], "Cour/Chariot", yaw=15)
prop("Chariot_Cabine", (-720, -1600, 175), (100, 100, 90), M["SteelDark"], "Cour/Chariot", yaw=15)
prop("Chariot_Mat", (-585, -1569, 150), (10, 90, 260), M["SteelDark"], "Cour/Chariot", yaw=15)
prop("Chariot_Fourche", (-530, -1555, 10), (110, 70, 6), M["SteelDark"], "Cour/Chariot", yaw=15)

# --- Entrepôt abandonné (x -1600..1600, y -3900..-1900, h 700) -------------
WX0, WX1, WY0, WY1, WH = -1600, 1600, -3900, -1900, 700
WIN0, WIN1 = 450, 600
F = "Entrepot"

block("Entrepot_MurN", WX0, -300, WY1 - T, WY1, 0, WIN0, M["ConcreteDirty"], F)
block("Entrepot_MurN", 300, WX1, WY1 - T, WY1, 0, WIN0, M["ConcreteDirty"], F)
prop("Rideau_Metallique", (0, WY1 - 15, 360), (600, 10, 180), M["MetalRust"], F)
block("Entrepot_MurO", WX0, WX0 + T, WY0, -2650, 0, WIN0, M["ConcreteDirty"], F)
block("Entrepot_MurO", WX0, WX0 + T, -2550, WY1, 0, WIN0, M["ConcreteDirty"], F)
block("Entrepot_MurO_Linteau", WX0, WX0 + T, -2650, -2550, 220, WIN0, M["ConcreteDirty"], F)
block("Entrepot_MurE", WX1 - T, WX1, WY0, -3100, 0, WIN0, M["ConcreteDirty"], F)
block("Entrepot_MurE", WX1 - T, WX1, -2850, WY1, 0, WIN0, M["ConcreteDirty"], F)
block("Entrepot_MurE_Breche", WX1 - T, WX1, -3100, -2850, 180, WIN0, M["ConcreteDirty"], F)
debris("Breche_Debris", WX1 - 80, -2975, 150, 12, M["ConcreteDirty"])
block("Entrepot_MurS", WX0, WX1, WY0, WY0 + T, 0, WIN0, M["ConcreteDirty"], F)
block("Entrepot_Bandeau", WX0, WX1, WY1 - T, WY1, WIN1, WH, M["ConcreteDirty"], F)
block("Entrepot_Bandeau", WX0, WX1, WY0, WY0 + T, WIN1, WH, M["ConcreteDirty"], F)
block("Entrepot_Bandeau", WX0, WX0 + T, WY0, WY1, WIN1, WH, M["ConcreteDirty"], F)
block("Entrepot_Bandeau", WX1 - T, WX1, WY0, WY1, WIN1, WH, M["ConcreteDirty"], F)
for x in range(WX0, WX1 + 1, 400):
    prop("Entrepot_Poteau", (x, WY1 - T / 2, (WIN0 + WIN1) / 2), (40, T, WIN1 - WIN0), M["ConcreteDirty"], F)
    prop("Entrepot_Poteau", (x, WY0 + T / 2, (WIN0 + WIN1) / 2), (40, T, WIN1 - WIN0), M["ConcreteDirty"], F)
for y in range(WY0, WY1 + 1, 400):
    prop("Entrepot_Poteau", (WX0 + T / 2, y, (WIN0 + WIN1) / 2), (T, 40, WIN1 - WIN0), M["ConcreteDirty"], F)
    prop("Entrepot_Poteau", (WX1 - T / 2, y, (WIN0 + WIN1) / 2), (T, 40, WIN1 - WIN0), M["ConcreteDirty"], F)
prop("Entrepot_Panneau", (-1000, WY1 + 6, 380), (500, 8, 90), M["SignBlue"], F)
holes = {(-800, -2700), (400, -3500)}
for x in range(WX0, WX1, 400):
    for y in range(WY0, WY1, 400):
        if (x, y) not in holes:
            prop("Entrepot_Toit", (x + 200, y + 200, WH + 15), (400, 400, 30), M["MetalRust"], F + "/Toit")
prop("Toit_Effondre", (-560, -2480, 60), (420, 380, 12), M["MetalRust"], F + "/Toit", pitch=18, roll=-8)
debris("Toit_Debris", 600, -3300, 180, 8, M["MetalRust"])
puddle(-600, -2500, 2, 300)
puddle(600, -3300, 2, 250)
block("Entrepot_Sol", WX0 + T, WX1 - T, WY0 + T, WY1 - T, -40, 2, M["ConcreteDirty"], F, max_piece=800)
for x in (-800, 0, 800):
    for y in (-2500, -3500):
        prop("Poteau_Acier", (x, y, WH / 2), (40, 40, WH), M["SteelDark"], F)
for (x, y) in ((-200, -2900), (700, -2300), (-1200, -2100)):
    prop("Tache_Huile", (x, y, 2.3), (random.uniform(80, 160), random.uniform(60, 120), 0.5), M["TrashBag"], F,
         yaw=random.uniform(0, 180), shadow=False)


def rack(label, x0, x1, y0, y1):
    """Rayonnage industriel : montants, 3 niveaux, cartons. Bon couvert."""
    for x in (x0, x1 - 8):
        for y in (y0, (y0 + y1) / 2, y1 - 8):
            prop(label + "_Montant", (x + 4, y + 4, 160), (8, 8, 320), M["Forklift"], F + "/Rayonnages")
    for z in (15, 120, 225, 315):
        prop(label + "_Tablette", ((x0 + x1) / 2, (y0 + y1) / 2, z), (x1 - x0, y1 - y0, 5), M["SteelDark"], F + "/Rayonnages")
    for z in (17.5, 122.5, 227.5):
        y = y0 + 40
        while y < y1 - 80:
            if random.random() < 0.7:
                s = random.uniform(50, 80)
                prop(label + "_Carton", ((x0 + x1) / 2, y + s / 2, z + s / 2), (s, s, s * 0.8), M["Cardboard"],
                     F + "/Rayonnages", yaw=random.uniform(-8, 8))
            y += 100


rack("Rayon1", -1350, -1250, -3700, -2300)
rack("Rayon2", -500, -400, -3700, -2900)
rack("Rayon3", 250, 350, -2700, -2100)

for (x, y, n) in ((-150, -2300, 2), (-100, -3200, 3), (500, -2400, 1), (1300, -2200, 2), (-1000, -3700, 2)):
    for k in range(n):
        crate("Entrepot_Caisse", x, y, k * 100, yaw=random.uniform(-10, 10))
pallet("Entrepot_Palette", 900, -2600)
pallet("Entrepot_Palette", 900, -2600, 14, yaw=20)
prop("Bache", (1250, -2650, 75), (220, 160, 150), M["Tarp"], F, yaw=8)
prop("Etabli", (-1480, -3000, 45), (90, 250, 90), M["Wood"], F)
for k in range(4):
    prop("Outil", (-1480 + random.uniform(-25, 25), -3000 + random.uniform(-100, 100), 93), (30, 10, 6),
         M["SteelDark"], F, yaw=random.uniform(0, 180), shadow=False)
for k in range(3):
    cylinder("Pneu_Entrepot", (1350, -3700, k * 25), 70, 25, M["Rubber"], F)
debris("Entrepot_Debris", -900, -3000, 300, 10, M["ConcreteDirty"])
debris("Entrepot_Cartons", 0, -2700, 250, 6, M["Cardboard"])
papers(-300, -2800, 600, 25, 2)

# mezzanine-bureau (x 900..1570, y -3870..-3100, plancher à 300)
MZ = 300
block("Mezzanine_Plancher", 900, WX1 - T, WY0 + T, -3100, MZ - 20, MZ, M["SteelDark"], F + "/Mezzanine")
for x in (910, 1550):
    prop("Mezzanine_Poteau", (x, -3110, MZ / 2), (20, 20, MZ), M["SteelDark"], F + "/Mezzanine")
block("Bureau_Facade", 900, 1100, -3130, -3100, MZ, MZ + 260, M["Plaster"], F + "/Mezzanine")
block("Bureau_Facade", 1400, WX1 - T, -3130, -3100, MZ, MZ + 260, M["Plaster"], F + "/Mezzanine")
block("Bureau_Allege", 1100, 1400, -3130, -3100, MZ, MZ + 100, M["Plaster"], F + "/Mezzanine")
block("Bureau_Linteau", 1100, 1400, -3130, -3100, MZ + 220, MZ + 260, M["Plaster"], F + "/Mezzanine")
block("Bureau_MurO", 900, 930, WY0 + T, -3450, MZ, MZ + 260, M["Plaster"], F + "/Mezzanine")
block("Bureau_MurO", 900, 930, -3250, -3130, MZ, MZ + 260, M["Plaster"], F + "/Mezzanine")
block("Bureau_MurO_Linteau", 900, 930, -3450, -3250, MZ + 210, MZ + 260, M["Plaster"], F + "/Mezzanine")
prop("Bureau_Table", (1300, -3500, MZ + 40), (160, 80, 80), M["Wood"], F + "/Mezzanine")
prop("Bureau_Ecran", (1300, -3530, MZ + 105), (50, 6, 35), E["WindowCold"], F + "/Mezzanine", shadow=False)
prop("Bureau_Armoire", (1500, -3800, MZ + 100), (100, 50, 200), M["SteelDark"], F + "/Mezzanine")
prop("Bureau_Chaise", (1300, -3420, MZ + 45), (50, 50, 90), M["Rubber"], F + "/Mezzanine", yaw=30)
papers(1200, -3600, 200, 12, MZ)
for i in range(15):
    h = (i + 1) * 20
    prop("Escalier_Marche", (900 - 27 * (15 - i) + 13.5, -3350, h / 2), (27, 180, h), M["SteelDark"], F + "/Mezzanine")
prop("Escalier_Rampe", (697, -3445, 250), (405, 6, 6), M["SteelDark"], F + "/Mezzanine", pitch=36.5)

# néons de l'entrepôt (suspendus à des chaînes) + poussière en suspension
for (x, y, pattern) in ((0, -2600, "FLICKER"), (-900, -3400, "FAULTY"), (600, -2200, "BUZZ")):
    for dx in (-60, 60):
        prop("Neon_Chaine", (x + dx, y, 600), (2, 2, 200), M["SteelDark"], F + "/Neons", shadow=False)
    flicker("Entrepot_Neon", (x, y, 495), pattern, (150, 14, 8), E["Tube"], 40, (0.8, 0.92, 1.0), 1300, 15,
            folder=F + "/Neons")
flicker("Bureau_Neon", (1250, -3500, MZ + 255), "BUZZ", (120, 12, 6), E["Tube"], 25, (0.85, 0.95, 1.0), 700, 12,
        folder=F + "/Mezzanine")
dust = unreal.load_asset("/Game/StarterContent/Particles/P_Ambient_Dust")
for loc in ((-600, -2500, 250), (500, -3300, 250), (0, -2200, 200), (-200, 1300, 200)):
    emitter = actors.spawn_actor_from_class(unreal.Emitter, unreal.Vector(*loc))
    emitter.get_component_by_class(unreal.ParticleSystemComponent).set_template(dust)
    emitter.set_actor_label("Poussiere")
    emitter.set_folder_path("Ambiance")

# --- Éclairage extérieur : nuit pluvieuse ------------------------------------
# Lune faible et froide : la rue est éclairée surtout par les lampadaires, les néons et les fenêtres.
moon = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 2000),
                                     unreal.Rotator(roll=0, pitch=-35, yaw=60))
moon.root_component.set_mobility(unreal.ComponentMobility.STATIONARY)
moon.light_component.set_editor_property("intensity", 0.15)
moon.light_component.set_light_color(unreal.LinearColor(0.55, 0.65, 1.0, 1.0))
moon.light_component.set_editor_property("dynamic_shadow_distance_stationary_light", 3000.0)
moon.set_actor_label("Lune")
moon.set_folder_path("Lumieres")
sky = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 1500))
sky.root_component.set_mobility(unreal.ComponentMobility.STATIONARY)
sky.light_component.set_editor_property("intensity", 0.4)
sky.set_actor_label("Ciel")
sky.set_folder_path("Lumieres")
actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0)).set_folder_path("Lumieres")
fog = actors.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0))
fog_comp = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
fog_comp.set_editor_property("fog_density", 0.05)
try:
    fog_comp.set_editor_property("fog_inscattering_luminance", unreal.LinearColor(0.015, 0.02, 0.03, 1.0))
except Exception as error:  # nom de propriété différent selon la version : on garde la valeur par défaut
    unreal.log_warning("Brouillard : couleur non appliquee (%s)" % error)
fog.set_folder_path("Lumieres")

# Pluie (C++ BodycamRain) : suit la caméra, coupée sous les toits
rain = actors.spawn_actor_from_class(unreal.load_class(None, "/Script/BodycamUE.BodycamRain"), unreal.Vector(0, 0, 0))
rain.get_editor_property("drops").set_material(0, bodycam_materials.get_rain_material())
rain.set_actor_label("Pluie")
rain.set_folder_path("Ambiance")

# --- Joueur + réglages de la carte -------------------------------------------
start = actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(-3200, 700, 120), unreal.Rotator(yaw=0))
start.set_actor_label("PlayerStart")
settings = world.get_world_settings()
settings.set_editor_property("default_game_mode", unreal.load_class(None, "/Script/BodycamUE.BodycamGameMode"))

unreal.log("BODYCAM_QUARTIER_ACTORS %d" % COUNT[0])
if unreal.EditorLoadingAndSavingUtils.save_map(world, MAP_PATH):
    unreal.log("BODYCAM_QUARTIER_OK " + MAP_PATH)
else:
    unreal.log_error("BODYCAM_QUARTIER_FAILED")
