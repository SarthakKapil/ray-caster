//game Loop and Rendering
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <stdio.h>
#include <set>
#include <chrono>
#include <vector>
#define PI 3.1415926535897932384

#include "GameMap.h"
#include "MapObject.h"
#include "custom_math.h"

const unsigned BLOCK_DIM = 64;
const unsigned TILESHIFT = 6;
const int DEPTH_OF_FIELD = 20;

const int SCREEN_WIDTH = 1920;
const int SCREEN_HEIGHT = 1080;

//the window on which everything is displayed
SDL_Window *window = NULL;
//the surface of the main screen
//SDL_Surface *screenSurf = NULL;

SDL_Renderer* renderer = NULL;

void castRays(GameMap *gMap, MapObject *player, SDL_Renderer *renderer, int angRange, int depth);
bool input(GameMap *gMap, MapObject *player, std::vector<MapObject*> &agent_arr,
			std::set<int> keys, double speed, double angVel, double dt);
bool checkWhiteBlock( GameMap *gMap, MapObject *player );

//Initializing SDL
bool init_SDL(){
	
	if( SDL_Init( SDL_INIT_VIDEO ) < 0 ){
		printf( "SDL couldnt initialize. SDL error: %s\n", SDL_GetError() );
		return false;
	}else{
		//creating window
		window = SDL_CreateWindow( "Ray Caster", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 
						SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN );
		
		if( window == NULL ){
			printf( "Window couldnt be created. SDL error: %s\n", SDL_GetError() );
			return false;
		}else{
			SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN);
			
			renderer = SDL_CreateRenderer ( window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC );
			//getting the window surface
			//screenSurf = SDL_GetWindowSurface( window );
			
			//renderer = SDL_GetRenderer( window );
		}
	}
	
	//Initialize PNG loading
	if( ( !( IMG_Init( IMG_INIT_PNG ) ) ) & IMG_INIT_PNG ) {
		printf( "SDL_image could not initialize! SDL_image Error: %s\n", IMG_GetError() );
		return false;
	}
	
	return true;
}

void close_SDL(){
	
	SDL_DestroyWindow( window );
	window = NULL;
	
	SDL_Quit();
	IMG_Quit();
}

void create_dark_walls(SDL_Surface *wall_textures, SDL_Surface *dark_wall_textures, double wallColorRatio){
	
	SDL_LockSurface(wall_textures);
	Uint8 *pixel = ((Uint8*)wall_textures->pixels);
	SDL_UnlockSurface(wall_textures);
	
	SDL_LockSurface(dark_wall_textures);
	Uint8 *dark_pixel = ((Uint8*)dark_wall_textures->pixels);
	SDL_UnlockSurface(dark_wall_textures);
	
	for( int i = 0; i < dark_wall_textures->h; i++ ){
		for( int j = 0; j < dark_wall_textures->pitch; j+=3 ){
			for( int k = 0; k < 3; k++ )
				dark_pixel[i*dark_wall_textures->pitch + j + k] 
					= (Uint8)( wallColorRatio*pixel[i*wall_textures->pitch + j + k] );
		}
	}
	
}

void frame_rate( SDL_Renderer *renderer, SDL_Texture *numbers, int fps ){
	
	if( fps > 99 )
		return;
	int digs[2];
	
	//extracting digits
	digs[0] = (int)( fps/10 );
	digs[1] = fps % 10;
	
	int wid, hig;
	SDL_GetRendererOutputSize( renderer, &wid, &hig );
	
	SDL_Rect srcRect, dstRect;
	srcRect.x = 5*digs[1]; srcRect.y = 0;
	srcRect.w = 4; srcRect.h = 7;
	
	//draw at the top right corner of the screen
	dstRect.x = wid - 25; dstRect.y = 0;
	dstRect.w = 20; dstRect.h = 35;
	SDL_RenderCopy( renderer, numbers, &srcRect, &dstRect );
	
	srcRect.x = 5*digs[0]; srcRect.y = 0;
	srcRect.w = 5; srcRect.h = 7;
	
	dstRect.x = wid - 50; dstRect.y = 0;
	dstRect.w = 25; dstRect.h = 35;
	
	SDL_RenderCopy( renderer, numbers, &srcRect, &dstRect );
	
}

int main( int argc, char* args[] ){
	
	if( !init_SDL() )
		return 0;
	
	SDL_Texture *numbers = IMG_LoadTexture( renderer, "./Images/numbers.bmp" );
	
	//the image holding all the wall textures
	SDL_Surface *wall_surfaces = SDL_LoadBMP("./Images/walls.bmp");
	
	if( wall_surfaces == NULL ){
		printf( "Wall textures couldnt be loaded. Error: %s\n", SDL_GetError() );
		return 0;
	}
	
	//the image holding the darkened versions of the wall textures
	SDL_Surface *dark_wall_surfaces = SDL_CreateRGBSurface( 0, wall_surfaces->w, wall_surfaces->h, 24, 0, 0, 0, 0 );
	
	//change the pixels in the dark texture image
	create_dark_walls( wall_surfaces, dark_wall_surfaces, 0.75 );
	
	SDL_Texture *wall_textures = SDL_CreateTextureFromSurface( renderer, wall_surfaces );
	SDL_Texture *dark_wall_textures = SDL_CreateTextureFromSurface( renderer, dark_wall_surfaces );
	
	SDL_FreeSurface( wall_surfaces );
	SDL_FreeSurface( dark_wall_surfaces );
	wall_surfaces = dark_wall_surfaces = NULL;
	
	
	//this is the map image that is used to load the map
	SDL_Surface *mapImg = SDL_LoadBMP("./Images/levelTrial5.bmp");
	//SDL_Surface *mapImg = SDL_LoadBMP("./Images/spriteTest.bmp");
	
	if( mapImg == NULL ){
		printf( "Map image couldnt be loaded. Error: %s\n", SDL_GetError() );
		SDL_DestroyTexture( wall_textures );
		SDL_DestroyTexture( dark_wall_textures );
		close_SDL();
		return 0;
	}
	
	//the sprite of the enemy
	SDL_Texture *spriteText = IMG_LoadTexture( renderer, "./Images/sprite3.bmp");
	
	if( spriteText == NULL ){
		printf( "Sprite not initialized. Error: %s\n", SDL_GetError() );
		SDL_DestroyTexture( wall_textures );
		SDL_DestroyTexture( dark_wall_textures );
		SDL_FreeSurface( mapImg );
		close_SDL();
		return 0;
	}
	
	
	Player player(80.0, 80.0, (7*PI)/2, 10);
	
	int agent_cnt = 1;
	
	SDL_LockSurface(mapImg);
	Uint8 *pixel = ((Uint8*)mapImg->pixels);
	SDL_UnlockSurface(mapImg);
	
	for( int i = 0; i < mapImg->h; i++ ){
		for( int j = 0; j < mapImg->w*3; j+=3 ){
			
			//finding number of agents on the map, RGB = (0, 255, 1) is an agent and RGB = (1, 0, 255) is a fast agent
			if( ( pixel[i*mapImg->pitch + j] == 1
				and pixel[i*mapImg->pitch + j + 1] == 255
				and pixel[i*mapImg->pitch + j + 2] == 0 )
				or
				( pixel[i*mapImg->pitch + j] == 255
				and pixel[i*mapImg->pitch + j + 1] == 0
				and pixel[i*mapImg->pitch + j + 2] == 1 )
				)
					agent_cnt += 1;
			
			//finding the player's block and assigning position, RGB = (255, 255, 254) is the player
			if( pixel[i*mapImg->pitch + j] == 254
				and pixel[i*mapImg->pitch + j + 1] == 255
				and pixel[i*mapImg->pitch + j + 2] == 255 ){
					
				player.x = (double)( ( (int)(j/3) << TILESHIFT ) + ( BLOCK_DIM >> 1 ));
				player.y = (double)( ( i << TILESHIFT ) + ( BLOCK_DIM >> 1 ) );
			}
		}
	}
	
	std::vector<MapObject*> agent_arr;
	agent_arr.push_back( &player );
	for( int i = 1; i < agent_cnt; i++ ){
		agent_arr.push_back( new Agent( spriteText, 0.0, 0.0, (7*PI)/2, 10, 10, 100.0, 1.5, &player ) );
	}
	
	int agent_index = 1;
	
	for( int i = 0; i < mapImg->h; i++ ){
		for( int j = 0; j < mapImg->w*3; j+=3 ){
			
			//finding the agents again and assigning them their positions
			if( pixel[i*mapImg->pitch + j] == 1
				and pixel[i*mapImg->pitch + j + 1] == 255
				and pixel[i*mapImg->pitch + j + 2] == 0 ){
					
				agent_arr[agent_index]->x = (double)( ( (int)(j/3) << TILESHIFT ) + ( BLOCK_DIM >> 1 ));
				agent_arr[agent_index]->y = (double)( ( i << TILESHIFT ) + ( BLOCK_DIM >> 1 ) );
				
				agent_index++;
			}
			
			else if( pixel[i*mapImg->pitch + j] == 255
				and pixel[i*mapImg->pitch + j + 1] == 0
				and pixel[i*mapImg->pitch + j + 2] == 1 ){
					
				agent_arr[agent_index]->x = (double)( ( (int)(j/3) << TILESHIFT ) + ( BLOCK_DIM >> 1 ));
				agent_arr[agent_index]->y = (double)( ( i << TILESHIFT ) + ( BLOCK_DIM >> 1 ) );
				agent_arr[agent_index]->double_speed();
				agent_arr[agent_index]->ang = PI*0.5;
				
				agent_index++;
			
			}
			
			if( agent_index == (int)agent_arr.size() )
				break;
		}
		if( agent_index == (int)agent_arr.size() )
			break;
	}
	
	//creating a map object
	GameMap gMap( mapImg, wall_textures, dark_wall_textures, 0.75 );
	//GameMap gMap("./Images/spriteTest.bmp");
	
	SDL_FreeSurface( mapImg );
	
	
	
	//to run the game loop
	bool running = true;
	
	//whether to run the game loop after the start screen
	bool run_game = true;
	
	//to show 3D scene or top down view
	bool show3D = true;
	
	//if eaten by monster
	bool game_over = false;
	
	//if you touched the white block
	bool game_won = false;
	
	//if you paused the game
	bool paused = false;
	
	//to check time diff between two frames
	//when the game is paused, time diff is not to be checked
	bool check_time = true;
	
	//to access events in the event queue
	SDL_Event e;
	
	//SDL_Surface *startScreen = SDL_LoadBMP("./Images/startScreen.bmp");
	SDL_Texture *startScreen = IMG_LoadTexture( renderer, "./Images/startScreen.bmp" );
	
	if( startScreen == NULL ){
		printf(" Couldnt load start screen. Error: %s\n", SDL_GetError() );
		SDL_DestroyTexture( wall_textures );
		SDL_DestroyTexture( dark_wall_textures );
		close_SDL();
		return 0;
	}
	
	//SDL_Texture *startTexture = SDL_CreateTextureFromSurface( startScreen );
	
	SDL_Texture *pauseScreen = IMG_LoadTexture( renderer, "./Images/paused.bmp" );
	
	if( pauseScreen == NULL ){
		printf(" Couldnt load pause screen. Error: %s\n", SDL_GetError() );
		SDL_DestroyTexture( wall_textures );
		SDL_DestroyTexture( dark_wall_textures );
		SDL_DestroyTexture( startScreen );
		return 0;
	}
	
	SDL_RenderClear( renderer );
	SDL_RenderCopy( renderer, startScreen, NULL, NULL );
	SDL_RenderPresent( renderer );
	
	SDL_DestroyTexture( startScreen );
	
	//to run start screen
	while( running ){
		
		while( SDL_PollEvent( &e ) != 0 ){
			
			switch( e.type ){
				
				case SDL_QUIT:
					running = false;
					//don't run the game loop, just quit
					run_game = false;
					break;
					
				case SDL_KEYDOWN:
					//continue to the game loop
					if( e.key.keysym.sym == SDLK_RETURN )
						running = false;
					break;
					
			}
		}
	}
	
	if( run_game )
		running = true;
	
	//to hold key presses
	std::set<int> keys = {};
	
	//to calculate time difference between each frame
	std::chrono::steady_clock::time_point oldTime = std::chrono::steady_clock::now();
	std::chrono::steady_clock::time_point newTime = std::chrono::steady_clock::now();
	
	int width, height;
	SDL_GetRendererOutputSize( renderer, &width, &height );
	
	//to show sky and ground in 3D scene
	SDL_Rect sky, ground;
	sky.x = 0; sky.y = 0; sky.h = height >> 1; sky.w = width;
	ground.x = 0; ground.y = height >> 1; ground.h = ground.y; ground.w = width;
	
	double total_time;
	total_time = 0;
	int avg_frame_rate = 60;
	int frames = 0;
	
	//MAIN+GAME+LOOP+++++++++++++++++MAIN+GAME+LOOP++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
	while( running ){
		
		//accessing events from the event queue
		while( SDL_PollEvent( &e ) != 0 ){
			
			//checking for quit, keypress and keylift events
			switch( e.type ){
				case SDL_QUIT:
					running = false;
					break;
					
				case SDL_KEYDOWN:
					
					switch( e.key.keysym.sym ){
						
						case SDLK_ESCAPE:
							paused = true;
							break;
						
						//switch between 3D and top down view if tab is pressed
						case SDLK_TAB:
							show3D = !show3D;
							break;
							
						default:
							//any other key presses are dealt with by the input function
							if( keys.size() < 5 )
								keys.insert( e.key.keysym.sym );
						
					}
					break;
					
					
				case SDL_KEYUP:
					//remove keys from set if they are lifted
					if( !keys.empty() )
						keys.erase(e.key.keysym.sym);
						
					break;
			}
		}
		
		if( paused ){
			
			//unpress all pressed keys and dont check time diff for the frame right after game has been un-paused
			keys.clear();
			check_time = false;
			
			SDL_SetRenderDrawColor( renderer, 0, 0, 0, 255 );
			SDL_RenderClear( renderer );
			SDL_RenderCopy( renderer, pauseScreen, NULL, NULL );
			SDL_RenderPresent( renderer );
			
			while( paused ){
				
				while( SDL_PollEvent( &e ) != 0 ){
					switch( e.type ){
						
						case SDL_QUIT:
							paused = false;
							running = false;
							break;
						
						case SDL_KEYDOWN:
						
							switch( e.key.keysym.sym ){
								
								case SDLK_ESCAPE:
									//no break, since it also needs to un-pause
									running = false;
								
								case SDLK_RETURN:
									paused = false;
									break;
							}
					}
				}
			}
			
		}
		
		//time diff between two frames
		double dt;
		//get time now
		newTime = std::chrono::steady_clock::now();
		
		if( check_time ){
			
			//calculate time difference in mus
			dt = std::chrono::duration_cast<std::chrono::microseconds>(newTime - oldTime).count();
			//convert to time difference in seconds
			dt *= 0.000001;
			
		}
		else{
			//no time diff
			dt = 0.0;
			//keep checking time diff after this frame
			check_time = true;
		}
		
		//to find time diff for the next fame
		oldTime = newTime;
		
		//run game logic
		if( !( game_over or game_won ) ){
			
			//get input from keyboard and move player according to it
			if( input( &gMap, &player, agent_arr, keys, 240, 1.5, dt ) ){
				game_won = true;
				break;
			}
			
			//move all the enemies
			for( int i = 1; i < (int)agent_arr.size(); i++ ){
				if( agent_arr[i]->follow_player( &gMap, agent_arr, dt ) )
					game_over = true;
			}
			
			if( show3D ){
				
				SDL_RenderClear( renderer );
				
				//sky is a light blue color
				SDL_SetRenderDrawColor( renderer, 69, 250, 254, 255 );
				SDL_RenderFillRect( renderer, &sky );
				//ground is a dark gray color
				SDL_SetRenderDrawColor( renderer, 50, 50, 50, 255 );
				SDL_RenderFillRect( renderer, &ground );
				
				//cast rays and draw the environment on the screen
				castRays( &gMap, &player, renderer, 30, DEPTH_OF_FIELD );
				
				//display the enemies on the screen
				//for( int i = 1; i < (int)agent_arr.size(); i++ )
				//	agent_arr[i]->sprite3D( &gMap, renderer, &player, 0.5235987755982988);
			
				draw_3D_sprites( renderer, &gMap, &player, agent_arr, 0.5235987755982988 );
				
			}else{
				
				//a black background
				SDL_SetRenderDrawColor( renderer, 0, 0, 0, 255 );
				SDL_RenderClear( renderer );
				
				//draw the 2D map top down view
				gMap.draw2DMap( renderer , player.x, player.y );
				
				//draw all the players on the map
				for( int i = 0; i < (int)agent_arr.size(); i++ )
					agent_arr[i]->sprite2D( renderer );
				}
		}else{
			//if game is over or won, then stop running the game loop
			running = false;
		}
		
		total_time += dt;
		frames += 1;
		
		if( total_time > 0.1 ){
			avg_frame_rate = (double)(frames)/total_time;
			frame_rate( renderer, numbers, avg_frame_rate );
			total_time = 0.0;
			frames = 0;
			if( avg_frame_rate < 40 )
				printf( "Very Low Framerate: %d\n", avg_frame_rate );
		}else{
			frame_rate( renderer, numbers, avg_frame_rate );
		}
		
		SDL_RenderPresent( renderer );
	}
	
	SDL_DestroyTexture( pauseScreen );
	
	//self explanatory
	if( game_over ){
		SDL_Texture *gameOverScreen = IMG_LoadTexture( renderer, "./Images/gameOver.bmp" );
		SDL_RenderClear( renderer );
		SDL_RenderCopy( renderer, gameOverScreen, NULL, NULL );
		SDL_RenderPresent( renderer );
		SDL_DestroyTexture( gameOverScreen );
		
	}else if( game_won ){
		SDL_Texture *gameWonScreen = IMG_LoadTexture( renderer, "./Images/gameWon.bmp" );
		SDL_RenderClear( renderer );
		SDL_RenderCopy( renderer, gameWonScreen, NULL, NULL );
		SDL_RenderPresent( renderer );
		SDL_DestroyTexture( gameWonScreen );
	}else{
		SDL_Texture *gameExit = IMG_LoadTexture( renderer, "./Images/gameExit.bmp" );
		SDL_RenderClear( renderer );
		SDL_RenderCopy( renderer, gameExit, NULL, NULL );
		SDL_RenderPresent( renderer );
		SDL_DestroyTexture( gameExit );
	}
	
	running = true;
	
	//this loop runs while displaying the final screen
	while( running ){
		
		while( SDL_PollEvent( &e ) != 0 ){
			
			switch( e.type ){
				case SDL_QUIT:
					
					running = false;
					break;
					
				case SDL_KEYDOWN:
					
					if( e.key.keysym.sym == SDLK_ESCAPE )
						running = false;
			
			}
		}
	}

	
	//wrapping up everything
	SDL_DestroyTexture( wall_textures );
	SDL_DestroyTexture( dark_wall_textures );
	close_SDL();
	
	return 0;
}

bool checkWhiteBlock( GameMap *gMap, MapObject *player ){
	int mapX, mapY;
	
	//finding where the point right in front of the player lies in the map array
	mapX = (int)( player->x + std::cos(player->ang)*(double)player->objDim ) >> TILESHIFT;
	mapY = (int)( player->y - std::sin(player->ang)*(double)player->objDim ) >> TILESHIFT;
	
	Block *block = gMap->block_at( mapY, mapX );
	
	if( block != NULL ){
		if( block->isWall ){
			//if this block is fully white, return true ( player wins )
			return block->colors[0] == 255 and block->colors[1] == 255 and block->colors[2] == 255;
		}
	}
	
	return false;
}

bool input(GameMap *gMap, MapObject *player, std::vector<MapObject*> &agent_arr,
			std::set<int> keys, double speed, double angVel, double dt){
	
	double moveX, moveY, moveAng;
	moveX = moveY = moveAng = 0.0;
	
	bool touched = false;
	
	if( keys.count(SDLK_SPACE) != 0 ){
		speed *= 2;
		angVel *= 2;
	}
	if( keys.count(SDLK_LSHIFT) != 0 or keys.count(SDLK_RSHIFT) != 0 ){
		speed /= 2;
		angVel /= 2;
	}
	
	for(int n : keys){
		switch(n){
			
			case SDLK_a:
				moveAng += angVel*dt;
				break;
			
			case SDLK_s:
				moveAng -= angVel*dt;
				break;
				
			case SDLK_UP:
				moveX += speed*dt;
				break;
			
			case SDLK_DOWN:
				moveX -= speed*dt;
				break;
			
			case SDLK_RIGHT:
				moveY += speed*dt;
				break;
			
			case SDLK_LEFT:
				moveY -= speed*dt;
				break;
				
			case SDLK_LCTRL:
			case SDLK_RCTRL:
				touched = checkWhiteBlock( gMap, player );

		}
	}
	
	player->move(gMap, agent_arr, moveX, moveY, moveAng, true);
	
	return touched;
}

void castRays(GameMap *gMap, MapObject *player, SDL_Renderer *renderer, int angRange, int depth){
	
	//dimensions of the screen
	int wid, hig;
	SDL_GetRendererOutputSize( renderer, &wid, &hig );
	
	//range of angles in which the rays are casted (in radians)
	double radAngRange = (PI*angRange)/180.0;
	//num of rays casted
	int rayCount = wid;
	
	//distance away from the player where the "screen" is situated.
	//all rays are projected onto this screen
	double screenDist = (double)wid/( 2*std::tan(radAngRange) );
	
	//the height of the slice of the wall where the ray hits
	double rayHig;
	
	//map indices where the ray hits the wall
	int vmapX, vmapY, hmapX, hmapY;
	
	vmapX = vmapY = hmapX = hmapY = 0;
	
	for( int i = 0; i < rayCount; i++ ){
		
		//angle is calculated by atan
		double rAng = mod2PI( player->ang + modPI( real_atan( screenDist, (double) ( ( wid >> 1 ) - i ) ) ) );
		
		double hDist, vDist;
		
		//the horiz offset at which a slice of the wall texture is to be
		//picked
		int v_offset, h_offset, offset_x;
		
		cast_horiz_ray( gMap, player->x, player->y, rAng, depth, &hmapX, &hmapY, &hDist, &h_offset );
		cast_vert_ray( gMap, player->x, player->y, rAng, depth, &vmapX, &vmapY, &vDist, &v_offset );
		
		//which among the two distances is smallest
		double finalDist;
		
		//the wall where the ray hit
		Block *wall = NULL;
		bool isVertical = false;
		
		if( vDist > hDist ){
			finalDist = hDist;
			offset_x = h_offset;
			wall = gMap->horiz_wall_at( hmapY, hmapX );
			
		}else{
			finalDist = vDist;
			offset_x = v_offset;
			wall = gMap->vert_wall_at( vmapY, vmapX );
			isVertical = true;
			
		}
		
		//removing fish eye effect
		finalDist *= std::cos(rAng - player->ang);
		
		if( finalDist == 0.0 ){
			//skip drawing this ray
			continue;
		}
		
		rayHig = ( (double)BLOCK_DIM * screenDist )/finalDist;
		
		//if slice height is lesser than screen height, texture doesn't get clipped
		//but if slice height is greater than, then the texture has to be clipped
		//from above and below to coincide with the field of view of the player
		int offset_y = 0;
		
		if( rayHig > hig ){
			offset_y = (int)(((rayHig - (double)hig)*(double)BLOCK_DIM)/(2.0*rayHig));
		}
		
		//capping ray height at screen height
		rayHig = rayHig > hig ? (double)hig : rayHig;
		
		SDL_Rect slice;
		slice.y = (hig - (int)rayHig) >> 1; slice.x = i;
		slice.h = (int)rayHig; slice.w = 1;
		
		wall->blit_wall_to_screen( renderer, &slice, offset_x, offset_y, isVertical );
		
	}
}