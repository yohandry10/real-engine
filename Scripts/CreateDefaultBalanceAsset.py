import unreal


ASSET_DIR = "/Game/Data/Balance"
ASSET_NAME = "DA_WLBalance"
ASSET_PATH = f"{ASSET_DIR}/{ASSET_NAME}"


def main():
    if not unreal.EditorAssetLibrary.does_directory_exist(ASSET_DIR):
        unreal.EditorAssetLibrary.make_directory(ASSET_DIR)

    asset = unreal.EditorAssetLibrary.load_asset(ASSET_PATH)
    if asset is None:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.WLBalanceDataAsset)
        asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
        asset = asset_tools.create_asset(
            ASSET_NAME,
            ASSET_DIR,
            unreal.WLBalanceDataAsset,
            factory,
        )

    if asset is None:
        raise RuntimeError(f"Could not create {ASSET_PATH}")

    unreal.EditorAssetLibrary.save_asset(ASSET_PATH, only_if_is_dirty=False)

    unreal.log(
        "WorldLeader default balance asset ready: "
        f"{asset.get_path_name()}. "
        "Assign it in Config/DefaultGame.ini under /Script/WorldLeader.WLBalanceSettings."
    )


if __name__ == "__main__":
    main()
