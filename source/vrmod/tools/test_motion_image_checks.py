"""Regression: reject black/one-eye/frozen output, including a surviving glove."""
from check_motion_camera import check_motion_images,Image
import copy

def pair(shift):
 out=[]
 for eye in (0,1):
  im=Image.new('RGB',(64,72));im.putdata([((x*3+shift+eye*5)%220+20,(y*3+shift)%220+20,90) for y in range(72) for x in range(64)]);out.append(im)
 return out

def accepted(images):return all(check_motion_images(images,[(64,72)]*2)[0].values())
valid={1:pair(0),2:pair(21)}
assert accepted(valid)
black=Image.new('RGB',(64,72))
assert not accepted({1:[black,black],2:pair(21)})
assert not accepted({1:[valid[1][0],black],2:pair(21)})
assert not accepted({1:[valid[1][0]]*2,2:pair(21)})
assert not accepted({1:pair(0),2:pair(0)})
assert not accepted({1:pair(0)})
glove=black.copy();glove.paste((180,90,30),(0,55,16,72))
checks,_=check_motion_images({1:[glove,glove],2:pair(21)},[(64,72)]*2)
assert checks['image_1_nonblack_nonuniform']
assert not checks['image_1_lit_test_scene_not_missing']
assert not accepted({1:[Image.new('RGB',(64,72),(128,128,128))]*2,2:pair(21)})
print('PASS motion image acceptance and 7 negative regressions (including partial glove black scene)')
