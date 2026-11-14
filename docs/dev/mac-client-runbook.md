# Runbook: building and testing the macOS client from a branch

How to build `feat/per-monitor-layout` on the Mac and run it as the client
of the Windows server, without disturbing the installed Deskflow more than
necessary.

You do **not** need the Xcode app or an Xcode project. The command line tools
plus CMake and Ninja are enough; Xcode is only needed for the optional
signing step at the end.

## 1. One-time setup

```sh
xcode-select --install          # Apple's compiler; skip if `clang --version` already works
brew install cmake ninja qt openssl@3
```

[Homebrew](https://brew.sh) is assumed. Qt must be 6.7 or newer; Homebrew's is.

## 2. Get the code

Download it from GitHub with git, which came with the command line tools in step 1:

```sh
git clone -b feat/per-monitor-layout https://github.com/npeaks/deskflow.git ~/src/deskflow
cd ~/src/deskflow
```

HTTPS needs no SSH key on the Mac. If GitHub asks you to sign in (only for a
private fork), use your GitHub username and a personal access token as the
password, or run `gh auth login` first if you have the GitHub CLI.

Already cloned? `git fetch && git checkout feat/per-monitor-layout && git pull`.

## 3. Configure and build

```sh
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$(brew --prefix qt)" \
  -DOPENSSL_ROOT_DIR="$(brew --prefix openssl@3)" \
  -DBUILD_INSTALLER=OFF \
  -DSKIP_BUILD_TESTS=ON
cmake --build build
```

The first configure takes a minute; the build a few more. The result is
`build/bin/Deskflow.app`, with `deskflow-core` inside it at
`Contents/MacOS/`. The app uses Homebrew's Qt and OpenSSL rather than
bundling them, so it only runs on this Mac.

**If the build fails**, save the first error and the lines around it:

```sh
cmake --build build 2>&1 | tee build.log
```

The macOS monitor code on this branch hasn't been compiled before, so an
error in `src/lib/platform/OSXComputer.mm` is the most likely failure.

## 4. Run it

1. Quit the installed Deskflow (menu bar icon → Quit) so only one copy runs.
2. `open build/bin/Deskflow.app`
3. Grant permissions when asked, or in **System Settings → Privacy & Security**:
   - **Accessibility**: add `build/bin/Deskflow.app` and switch it on.
   - **Input Monitoring**: same.

   If Deskflow is already listed, remove it with **–** and add the new
   build. The entry for the installed app doesn't cover this build.
4. The dev build uses the same settings as the installed app, so it should
   already be set to **client**, with the right server address and the
   computer name the server knows (e.g. `mac`). Check that this name matches
   the one in the server's Layout tab.
5. In **Settings**, set the log level to **Debug**, then start the client.

## 5. Confirm it's the new code

Within a few seconds of connecting:

- The Mac log shows its monitors in points, e.g.
  `monitors: 1: 0,0 1512x982`.
- The Windows log shows
  `received client "mac" monitors: ...`.
- On Windows, **Configure Server → Layout** draws the Mac with its real
  shape (solid border, no "not seen yet").

If the Windows server never logs `received client "mac" monitors`, one side
is still running an older build. Check that no installed copy is running on
either machine.

## 6. Test

With the Mac arranged in the Layout tab and the server config saved:

- Down from the landscape 4K where the Mac sits under it → the cursor
  enters the Mac's top edge at the matching spot.
- Down from the landscape past the Mac's right end → nothing happens.
- Right from the lower part of the portrait 4K → the Mac's left edge.
- Up and left from the Mac → back onto the matching 4K.
- Right and bottom edges of the Mac → nothing happens.
- The cursor doesn't drift or stick while moving around on the Mac.

Every crossing logs `switch from "win" to "mac" at x,y` on Windows. When
something is off, copy the log lines around it from both machines.

## 7. Picking up fixes

```sh
cd ~/src/deskflow
git pull
cmake --build build
```

Quit the running build first. After each rebuild, macOS may ask for
Accessibility and Input Monitoring again: remove the old entry and add the
app again.

## 8. Optional: stop re-granting permissions after every build

macOS ties the permissions to the app's code signature, and an unsigned
build gets a new one every time. Signing with a free Apple ID keeps it
stable:

1. Install **Xcode** from the App Store. Open it, then go to **Settings → Accounts**
   and add your Apple ID.
2. **Manage Certificates → + → Apple Development**.
3. `security find-identity -v -p codesigning login.keychain-db` and copy
   the `Apple Development: … (XXXXXXXXXX)` string.
4. Re-run step 3 with
   `-DAPPLE_CODESIGN_DEV="Apple Development: you@example.com (XXXXXXXXXX)"`
   added, and build again.

## 9. Going back to the installed version

Quit the dev build and start the installed Deskflow. Your settings are
shared, so nothing else needs changing. If the installed app lost its
permissions, remove and re-add it in Privacy & Security.
