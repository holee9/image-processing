// Read-only inventory of pinned Git objects for issue #276. Lexical matches are not compliance verdicts.
const cp = require('child_process');
const cwd = 'D:/workspace-github/image-processing';
const pins = { main:'d06de68cbd2a5d68083cb548031800b6fbef0cdd', pre:'75c0043195e5afacf7d6b424bee32c063b9e99a0', post:'d0e66d774d4ebdae0d57d014dfdba35c999b4dc0', gui:'541ee6d17b7979292b866536be424552214f6da5', doc:'ceb088a44076642183790fdd056ea767e7720c81' };
function git(args,input) { return cp.execFileSync('git',args,{cwd,input,maxBuffer:180*1024*1024}); }
function tree(sha) { return git(['ls-tree','-r',sha]).toString().trim().split('\n').map(l=>{let m=l.match(/^\d+ blob (\w+)\t(.+)$/);return m?{oid:m[1],path:m[2]}:null;}).filter(Boolean); }
const trees=Object.fromEntries(Object.entries(pins).map(([k,v])=>[k,tree(v)]));
const select=p=>/^(docs\/.*\.md|\.moai\/specs\/.*(?:spec|acceptance)\.md|(?:modules|clients|gui)\/.*\.(?:cpp|hpp|h|cs|xaml|csproj|txt)|\.github\/workflows\/.*ya?ml)$/.test(p);
const objects=new Map();for(const t of Object.values(trees))for(const f of t)if(select(f.path))objects.set(f.oid,null);
const ids=[...objects.keys()];const buf=git(['cat-file','--batch'],ids.join('\n')+'\n');let pos=0;
for(const id of ids){let end=buf.indexOf(10,pos);let h=buf.subarray(pos,end).toString().split(' ');let len=Number(h[2]);pos=end+1;objects.set(id,buf.subarray(pos,pos+len).toString('utf8'));pos+=len+1;}
const files=new Map(trees.main.filter(f=>select(f.path)).map(f=>[f.path,{...f,lane:'main'}]));
// Use the latest owned lane for its module/client/doc domain; no synthetic merge is performed.
for(const [lane,re] of [['pre',/^modules\/(common|preprocess)\//],['post',/^modules\/(ai|dicom|display|enhance_basic|enhance_advanced|gsvg)\//],['gui',/^(clients|gui)\//],['doc',/^docs\//]])for(const f of trees[lane])if(select(f.path)&&re.test(f.path))files.set(f.path,{...f,lane});
const entries=[...files.values()].map(f=>({...f,text:objects.get(f.oid)}));
const code=entries.filter(f=>/^(modules|clients|gui)\//.test(f.path)&&!/\/tests?\/|Tests\//i.test(f.path));
const tests=entries.filter(f=>/\/tests?\/|Tests\//i.test(f.path));
const defs=entries.filter(f=>/^docs\/.*SRS.*\.md$|^\.moai\/specs\/.*\/spec\.md$/.test(f.path));
const matrix=[];const idre=/\b(?:SRS|REQ|FR|SAF|PERF|NFR|IR|SR)-[A-Z0-9]+(?:-[A-Z0-9]+)*\b/g;
for(const d of defs){const seen=new Set();d.text.split('\n').forEach((line,i)=>{if(!/^\s*(?:\|\s*(?:\*\*)?|#{2,6}\s+(?:\*\*)?(?:\[)?|\*\*)(?:SRS-|REQ-|FR-|SAF-|PERF-|NFR-|IR-|SR-)/.test(line))return;const id=(line.match(idre)||[])[0];if(!id||seen.has(id))return;seen.add(id);const hit=arr=>arr.filter(f=>f.text.includes(id)).map(f=>f.path);const impl=hit(code),test=hit(tests),doc=entries.filter(f=>f.path.startsWith('docs/')&&f.path!==d.path&&f.text.includes(id)).map(f=>f.path);matrix.push({id,source:d.path,line:i+1,lane:d.lane,text:line.slice(0,1000),impl,test,doc,status:'unverified: lexical references are not proof'});});}
const links=[];for(const f of entries.filter(f=>f.path.startsWith('docs/'))){f.text.split('\n').forEach((l,i)=>{for(const m of l.matchAll(/\[[^\]]*\]\(([^)]+)\)/g)){let target=m[1].replace(/^<|>$/g,'').split('#')[0];if(!target||/^(?:https?:|mailto:|app:)/.test(target))continue;try{target=decodeURIComponent(target);}catch{}let p=require('path').posix.normalize(require('path').posix.join(require('path').posix.dirname(f.path),target));if(!trees.doc.some(x=>x.path===p)&&!trees.main.some(x=>x.path===p))links.push({source:f.path,line:i+1,target});}});}
const summary={pins,files:entries.length,docs:entries.filter(f=>f.path.startsWith('docs/')).length,definitions:defs.length,requirements:matrix.length,zeroCodeCitations:matrix.filter(x=>!x.impl.length).length,zeroTestCitations:matrix.filter(x=>!x.test.length).length,brokenLinks:links.length};
const delta={};for(const [k,v]of Object.entries(pins))if(k!=='main')delta[k]=git(['diff','--name-only',pins.main+'...'+v,'--','modules','clients','gui','docs']).toString().trim().split('\n').filter(Boolean);
console.log(JSON.stringify({summary,delta,matrix,links}));
